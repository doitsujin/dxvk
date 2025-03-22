#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

#include "../util/thread.h"

#include "dxvk_device.h"
#include "dxvk_context.h"

namespace dxvk {

  constexpr static size_t DxvkCsChunkSize = 16384;

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
     * \brief Retrieves pointer to next chain
     *
     * Used to chain commands.
     * \returns Pointer the next command
     */
    DxvkCsCmd** chain() {
      return &m_next;
    }

    /**
     * \brief Executes embedded commands
     * \param [in] ctx The target context
     */
    virtual void exec(DxvkContext* ctx) = 0;

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
  class DxvkCsTypedCmd : public DxvkCsCmd {
    
  public:
    
    DxvkCsTypedCmd(T&& cmd)
    : m_command(std::move(cmd)) { }
    
    DxvkCsTypedCmd             (DxvkCsTypedCmd&&) = delete;
    DxvkCsTypedCmd& operator = (DxvkCsTypedCmd&&) = delete;
    
    void exec(DxvkContext* ctx) {
      m_command(ctx);
    }
    
  private:
    
    T m_command;
    
  };


  /**
   * \brief Command data block
   *
   * Provides functionality to allocate a potentially growing
   * array of structures for a command to traverse.
   */
  class DxvkCsDataBlock {
    friend class DxvkCsChunk;
  public:

    /**
     * \brief Number of structures allocated
     * \returns Number of structures allocated
     */
    size_t count() const {
      return m_structCount;
    }

    /**
     * \brief Retrieves pointer to first structure
     * \returns Untyped pointer to first structure
     */
    void* first() {
      return reinterpret_cast<char*>(this) + m_dataOffset;
    }

    /**
     * \brief Retrieves pointer to given structure
     *
     * \param [in] idx Structure index
     * \returns Untyped pointer to given structure
     */
    void* at(uint32_t idx) {
      return reinterpret_cast<char*>(this) + m_dataOffset + idx * uint32_t(m_structSize);
    }

  private:

    uint32_t m_dataOffset  = 0u;
    uint16_t m_structSize  = 0u;
    uint16_t m_structCount = 0u;

  };


  /**
   * \brief Typed command with metadata
   * 
   * Stores a function object and an arbitrary
   * data structure which can be modified after
   * submitting the command to a cs chunk.
   */
  template<typename T, typename M>
  class DxvkCsDataCmd : public DxvkCsCmd {

  public:

    DxvkCsDataCmd(T&& cmd)
    : m_command(std::move(cmd)) { }

    ~DxvkCsDataCmd() {
      auto data = reinterpret_cast<M*>(m_data.first());

      for (size_t i = 0; i < m_data.count(); i++)
        data[i].~M();
    }

    DxvkCsDataCmd             (DxvkCsDataCmd&&) = delete;
    DxvkCsDataCmd& operator = (DxvkCsDataCmd&&) = delete;

    void exec(DxvkContext* ctx) {
      // No const here so that the function can move objects efficiently
      m_command(ctx, reinterpret_cast<M*>(m_data.first()), m_data.count());
    }

    DxvkCsDataBlock* data() {
      return &m_data;
    }

  private:

    alignas(std::max(alignof(T), alignof(M)))
    T               m_command;
    DxvkCsDataBlock m_data;

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
      void* ptr = alloc<FuncType>(0u);

      if (unlikely(!ptr))
        return false;

      auto next = new (ptr) FuncType(std::move(command));
      append(next);
      return true;
    }

    template<typename T>
    bool push(T&& command) {
      return push(command);
    }

    /**
     * \brief Adds a command with data to the chunk 
     * 
     * \param [in] command The command to add
     * \param [in] count Number of items to allocate. Should be at least
     *    1 in order to avoid the possibility of an empty command. Note
     *    that all allocated structures \e must be initialized before
     *    handing off the command to the worker thread.
     * \returns Pointer to the data object, or \c nullptr
     */
    template<typename M, typename T>
    DxvkCsDataBlock* pushCmd(T& command, size_t count) {
      size_t dataSize = count * sizeof(M);

      // DxvkCsDataCmd is aligned to M
      using FuncType = DxvkCsDataCmd<T, M>;
      void* ptr = alloc<FuncType>(dataSize);

      if (unlikely(!ptr))
        return nullptr;

      // Command data is always packed tightly after the function object
      auto next = new (ptr) FuncType(std::move(command));
      append(next);

      // Do some cursed pointer math here so that the block can figure out
      // where its data is stored based on its own address. This saves a
      // decent amount of CS chunk memory compared to storing a pointer.
      auto block = next->data();
      block->m_dataOffset = reinterpret_cast<uintptr_t>(&m_data[m_commandOffset - dataSize])
                          - reinterpret_cast<uintptr_t>(block);
      block->m_structSize = sizeof(M);
      block->m_structCount = count;
      return block;
    }

    /**
     * \brief Allocates more storage for a data block
     *
     * The data bock \e must be owned by the last command added to
     * the CS chunk, or this may override subsequent command data.
     * \param [in] block Data block
     * \param [in] count Number of structures to allocate
     * \returns Pointer to first allocated structure, or \c nullptr
     */
    void* pushData(DxvkCsDataBlock* block, uint32_t count) {
      uint32_t dataSize = block->m_structSize * count;

      if (unlikely(m_commandOffset + dataSize > DxvkCsChunkSize))
        return nullptr;

      void* ptr = &m_data[m_commandOffset];
      m_commandOffset += dataSize;

      block->m_structCount += count;
      return ptr;
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
    
    DxvkCsCmd*  m_head = nullptr;
    DxvkCsCmd** m_next = &m_head;

    DxvkCsChunkFlags m_flags;
    
    alignas(64)
    char m_data[DxvkCsChunkSize];

    template<typename T>
    void* alloc(size_t extra) {
      if (alignof(T) > alignof(DxvkCsCmd))
        m_commandOffset = dxvk::align(m_commandOffset, alignof(T));

      if (unlikely(m_commandOffset + sizeof(T) + extra > DxvkCsChunkSize))
        return nullptr;

      void* result = &m_data[m_commandOffset];
      m_commandOffset += sizeof(T) + extra;
      return result;
    }

    void append(DxvkCsCmd* cmd) {
      *m_next = cmd;
      m_next = cmd->chain();
    }
    
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
    
    dxvk::mutex               m_mutex;
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
    
    explicit operator bool () const {
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
   * \brief Queue type
   */
  enum class DxvkCsQueue : uint32_t {
    Ordered       = 0,  /// Normal queue with ordering guarantees
    HighPriority  = 1,  /// High-priority queue
  };


  /**
   * \brief Queued chunk entry
   */
  struct DxvkCsQueuedChunk {
    DxvkCsChunkRef  chunk;
    uint64_t        seq;
  };


  /**
   * \brief Chunk queue
   *
   * Stores queued chunks as well as the sequence
   * counters for synchronization.
   */
  struct DxvkCsChunkQueue {
    std::vector<DxvkCsQueuedChunk> queue;
    uint64_t                       seqDispatch = 0u;
  };


  /**
   * \brief Command stream thread
   * 
   * Spawns a thread that will execute
   * commands on a DXVK context. 
   */
  class DxvkCsThread {

  public:

    constexpr static uint64_t SynchronizeAll = ~0ull;

    DxvkCsThread(
      const Rc<DxvkDevice>&   device,
      const Rc<DxvkContext>&  context);
    ~DxvkCsThread();
    
    /**
     * \brief Dispatches an entire chunk
     * 
     * Can be used to efficiently play back large
     * command lists recorded on another thread.
     * \param [in] chunk The chunk to dispatch
     * \returns Sequence number of the submission
     */
    uint64_t dispatchChunk(DxvkCsChunkRef&& chunk);

    /**
     * \brief Injects chunk into the command stream
     *
     * This is meant to be used when serialized execution is required
     * from a thread other than the main thread recording rendering
     * commands. The context can still be safely accessed, but chunks
     * will not be executed in any particular oder. These chunks also
     * do not contribute to the main timeline.
     * \param [in] queue Which queue to add the chunk to
     * \param [in] chunk The chunk to dispatch
     * \param [in] synchronize Whether to wait for execution to complete
     */
    void injectChunk(
            DxvkCsQueue       queue,
            DxvkCsChunkRef&&  chunk,
            bool              synchronize);

    /**
     * \brief Synchronizes with the thread
     * 
     * This waits for all chunks in the dispatch queue to
     * be processed by the thread, up to the given sequence
     * number. If the sequence number is 0, this will wait
     * for all pending chunks to complete execution.
     * \param [in] seq Sequence number to wait for.
     */
    void synchronize(uint64_t seq);
    
    /**
     * \brief Retrieves last executed sequence number
     *
     * Can be used to avoid synchronization in some cases.
     * \returns Sequence number of last executed chunk
     */
    uint64_t lastSequenceNumber() const {
      return m_seqOrdered.load(std::memory_order_acquire);
    }

  private:

    Rc<DxvkDevice>              m_device;
    Rc<DxvkContext>             m_context;

    alignas(CACHE_LINE_SIZE)
    dxvk::mutex                 m_counterMutex;

    std::atomic<uint64_t>       m_seqHighPrio = { 0u };
    std::atomic<uint64_t>       m_seqOrdered  = { 0u };

    std::atomic<bool>           m_stopped     = { false };
    std::atomic<bool>           m_hasHighPrio = { false };

    alignas(CACHE_LINE_SIZE)
    dxvk::mutex                 m_mutex;
    dxvk::condition_variable    m_condOnAdd;
    dxvk::condition_variable    m_condOnSync;

    DxvkCsChunkQueue            m_queueOrdered;
    DxvkCsChunkQueue            m_queueHighPrio;

    dxvk::thread                m_thread;

    auto& getQueue(DxvkCsQueue which) {
      return which == DxvkCsQueue::Ordered
        ? m_queueOrdered : m_queueHighPrio;
    }

    auto& getCounter(DxvkCsQueue which) {
      return which == DxvkCsQueue::Ordered
        ? m_seqOrdered : m_seqHighPrio;
    }

    void threadFunc();
    
  };

}
