#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

#include "../util/thread.h"
#include "dxvk_context.h"

namespace dxvk {
  
  /**
   * \brief Command stream operation
   * 
   * An abstract representation of an operation
   * that can be recorded into a command list.
   */
  class DxvkCsCmd {
    
  public:
    
    virtual ~DxvkCsCmd() { }
    
    /**
     * \brief Retrieves next command in a command chain
     * 
     * This can be used to quickly iterate
     * over commands within a chunk.
     * \returns Pointer the next command
     */
    DxvkCsCmd* next() const {
      return m_next;
    }
    
    /**
     * \brief Sets next command in a command chain
     * \param [in] next Next command
     */
    void setNext(DxvkCsCmd* next) {
      m_next = next;
    }
    
    /**
     * \brief Executes embedded commands
     * \param [in] ctx The target context
     */
    virtual void exec(DxvkContext* ctx) const = 0;
    
  private:
    
    DxvkCsCmd* m_next = nullptr;
    
  };
  
  
  /**
   * \brief Typed command
   * 
   * Stores a function object which is
   * used to execute an embedded command.
   */
  template<typename T>
  class alignas(16) DxvkCsTypedCmd : public DxvkCsCmd {
    
  public:
    
    DxvkCsTypedCmd(T&& cmd)
    : m_command(std::move(cmd)) { }
    
    DxvkCsTypedCmd             (DxvkCsTypedCmd&&) = delete;
    DxvkCsTypedCmd& operator = (DxvkCsTypedCmd&&) = delete;
    
    void exec(DxvkContext* ctx) const {
      m_command(ctx);
    }
    
  private:
    
    T m_command;
    
  };


  /**
   * \brief Typed command with metadata
   * 
   * Stores a function object and an arbitrary
   * data structure which can be modified after
   * submitting the command to a cs chunk.
   */
  template<typename T, typename M>
  class alignas(16) DxvkCsDataCmd : public DxvkCsCmd {

  public:

    template<typename... Args>
    DxvkCsDataCmd(T&& cmd, Args&&... args)
    : m_command (std::move(cmd)),
      m_data    (std::forward<Args>(args)...) { }
    
    DxvkCsDataCmd             (DxvkCsDataCmd&&) = delete;
    DxvkCsDataCmd& operator = (DxvkCsDataCmd&&) = delete;

    void exec(DxvkContext* ctx) const {
      m_command(ctx, &m_data);
    }

    M* data() {
      return &m_data;
    }

  private:

    T m_command;
    M m_data;

  };
  
  
  /**
   * \brief Submission flags
   */
  enum class DxvkCsChunkFlag : uint32_t {
    /// Indicates that the submitted chunk will
    /// no longer be needed after one submission.
    SingleUse,
  };

  using DxvkCsChunkFlags = Flags<DxvkCsChunkFlag>;
  
  
  /**
   * \brief Command chunk
   * 
   * Stores a list of commands.
   */
  class DxvkCsChunk : public RcObject {
    constexpr static size_t MaxBlockSize = 16384;
  public:
    
    DxvkCsChunk();
    ~DxvkCsChunk();
    
    /**
     * \brief Checks whether the chunk is empty
     * \returns \c true if the chunk is empty
     */
    bool empty() const {
      return m_commandOffset == 0;
    }

    /**
     * \brief Tries to add a command to the chunk
     * 
     * If the given command can be added to the chunk, it
     * will be consumed. Otherwise, a new chunk must be
     * created which is large enough to hold the command.
     * \param [in] command The command to add
     * \returns \c true on success, \c false if
     *          a new chunk needs to be allocated
     */
    template<typename T>
    bool push(T& command) {
      using FuncType = DxvkCsTypedCmd<T>;
      
      if (unlikely(m_commandOffset > MaxBlockSize - sizeof(FuncType)))
        return false;
      
      DxvkCsCmd* tail = m_tail;
      
      m_tail = new (m_data + m_commandOffset)
        FuncType(std::move(command));
      
      if (likely(tail != nullptr))
        tail->setNext(m_tail);
      else
        m_head = m_tail;
      
      m_commandOffset += sizeof(FuncType);
      return true;
    }

    /**
     * \brief Adds a command with data to the chunk 
     * 
     * \param [in] command The command to add
     * \param [in] args Constructor args for the data object
     * \returns Pointer to the data object, or \c nullptr
     */
    template<typename M, typename T, typename... Args>
    M* pushCmd(T& command, Args&&... args) {
      using FuncType = DxvkCsDataCmd<T, M>;
      
      if (unlikely(m_commandOffset > MaxBlockSize - sizeof(FuncType)))
        return nullptr;
      
      FuncType* func = new (m_data + m_commandOffset)
        FuncType(std::move(command), std::forward<Args>(args)...);
      
      if (likely(m_tail != nullptr))
        m_tail->setNext(func);
      else
        m_head = func;
      m_tail = func;

      m_commandOffset += sizeof(FuncType);
      return func->data();
    }
    
    /**
     * \brief Initializes chunk for recording
     * \param [in] flags Chunk flags
     */
    void init(DxvkCsChunkFlags flags);
    
    /**
     * \brief Executes all commands
     * 
     * This will also reset the chunk
     * so that it can be reused.
     * \param [in] ctx The context
     */
    void executeAll(DxvkContext* ctx);
    
    /**
     * \brief Resets chunk
     * 
     * Destroys all recorded commands and
     * marks the chunk itself as empty, so
     * that it can be reused later.
     */
    void reset();
    
  private:
    
    size_t m_commandOffset = 0;
    
    DxvkCsCmd* m_head = nullptr;
    DxvkCsCmd* m_tail = nullptr;

    DxvkCsChunkFlags m_flags;
    
    alignas(64)
    char m_data[MaxBlockSize];
    
  };
  
  
  /**
   * \brief Chunk pool
   * 
   * Implements a pool of CS chunks which can be
   * recycled. The goal is to reduce the number
   * of dynamic memory allocations.
   */
  class DxvkCsChunkPool {
    
  public:
    
    DxvkCsChunkPool();
    ~DxvkCsChunkPool();
    
    DxvkCsChunkPool             (const DxvkCsChunkPool&) = delete;
    DxvkCsChunkPool& operator = (const DxvkCsChunkPool&) = delete;
    
    /**
     * \brief Allocates a chunk
     * 
     * Takes an existing chunk from the pool,
     * or creates a new one if necessary.
     * \param [in] flags Chunk flags
     * \returns Allocated chunk object
     */
    DxvkCsChunk* allocChunk(DxvkCsChunkFlags flags);
    
    /**
     * \brief Releases a chunk
     * 
     * Resets the chunk and adds it to the pool.
     * \param [in] chunk Chunk to release
     */
    void freeChunk(DxvkCsChunk* chunk);
    
  private:
    
    sync::Spinlock            m_mutex;
    std::vector<DxvkCsChunk*> m_chunks;
    
  };
  
  
  /**
   * \brief Chunk reference
   * 
   * Implements basic reference counting for
   * CS chunks and returns them to the pool
   * as soon as they are no longer needed.
   */
  class DxvkCsChunkRef {
    
  public:
    
    DxvkCsChunkRef() { }
    DxvkCsChunkRef(
      DxvkCsChunk*      chunk,
      DxvkCsChunkPool*  pool)
    : m_chunk (chunk),
      m_pool  (pool) {
      this->incRef();
    }
    
    DxvkCsChunkRef(const DxvkCsChunkRef& other)
    : m_chunk (other.m_chunk),
      m_pool  (other.m_pool) {
      this->incRef();
    }
    
    DxvkCsChunkRef(DxvkCsChunkRef&& other)
    : m_chunk (other.m_chunk),
      m_pool  (other.m_pool) {
      other.m_chunk = nullptr;
      other.m_pool  = nullptr;
    }
    
    DxvkCsChunkRef& operator = (const DxvkCsChunkRef& other) {
      other.incRef();
      this->decRef();
      this->m_chunk = other.m_chunk;
      this->m_pool  = other.m_pool;
      return *this;
    }
    
    DxvkCsChunkRef& operator = (DxvkCsChunkRef&& other) {
      this->decRef();
      this->m_chunk = other.m_chunk;
      this->m_pool  = other.m_pool;
      other.m_chunk = nullptr;
      other.m_pool  = nullptr;
      return *this;
    }
    
    ~DxvkCsChunkRef() {
      this->decRef();
    }
    
    DxvkCsChunk* operator -> () const {
      return m_chunk;
    }
    
    operator bool () const {
      return m_chunk != nullptr;
    }
    
  private:
    
    DxvkCsChunk*      m_chunk = nullptr;
    DxvkCsChunkPool*  m_pool  = nullptr;
    
    void incRef() const {
      if (m_chunk != nullptr)
        m_chunk->incRef();
    }
    
    void decRef() const {
      if (m_chunk != nullptr && m_chunk->decRef() == 0)
        m_pool->freeChunk(m_chunk);
    }
    
  };


  /**
   * \brief Command stream thread
   * 
   * Spawns a thread that will execute
   * commands on a DXVK context. 
   */
  class DxvkCsThread {
    
  public:
    
    DxvkCsThread(const Rc<DxvkContext>& context);
    ~DxvkCsThread();
    
    /**
     * \brief Dispatches an entire chunk
     * 
     * Can be used to efficiently play back large
     * command lists recorded on another thread.
     * \param [in] chunk The chunk to dispatch
     */
    void dispatchChunk(DxvkCsChunkRef&& chunk);
    
    /**
     * \brief Synchronizes with the thread
     * 
     * This waits for all chunks in the dispatch
     * queue to be processed by the thread. Note
     * that this does \e not implicitly call
     * \ref flush.
     */
    void synchronize();
    
    /**
     * \brief Checks whether the worker thread is busy
     * 
     * Note that this information is only reliable if
     * only the calling thread dispatches jobs to the
     * worker queue and if the result is \c false.
     * \returns \c true if there is still work to do
     */
    bool isBusy() const {
      return m_chunksPending.load() != 0;
    }
    
  private:
    
    const Rc<DxvkContext>       m_context;
    
    std::atomic<bool>           m_stopped = { false };
    dxvk::mutex                 m_mutex;
    dxvk::condition_variable    m_condOnAdd;
    dxvk::condition_variable    m_condOnSync;
    std::queue<DxvkCsChunkRef>  m_chunksQueued;
    std::atomic<uint32_t>       m_chunksPending = { 0u };
    dxvk::thread                m_thread;
    
    void threadFunc();
    
  };
  
}
