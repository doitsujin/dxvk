#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

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
     * \brief Executes embedded commands
     * \param [in] ctx The target context
     */
    virtual void exec(DxvkContext* ctx) const = 0;
    
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
   * \brief Command chunk
   * 
   * Stores a list of commands.
   */
  class DxvkCsChunk : public RcObject {
    constexpr static size_t MaxCommands  = 64;
    constexpr static size_t MaxBlockSize = 64 * MaxCommands;
  public:
    
    DxvkCsChunk();
    ~DxvkCsChunk();
    
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
      
      if (m_commandCount >= MaxCommands
       || m_commandOffset + sizeof(FuncType) > MaxBlockSize)
        return false;
      
      m_commandList[m_commandCount] =
        new (m_data + m_commandOffset)
          FuncType(std::move(command));
      
      m_commandCount  += 1;
      m_commandOffset += sizeof(FuncType);
      return true;
    }
    
    /**
     * \brief Executes all commands
     * 
     * This will also reset the chunk
     * so that it can be reused.
     * \param [in] ctx The context
     */
    void executeAll(DxvkContext* ctx);
    
  private:
    
    size_t m_commandCount  = 0;
    size_t m_commandOffset = 0;
    
    std::array<DxvkCsCmd*, MaxCommands> m_commandList;
    
    alignas(64)
    char m_data[MaxBlockSize];
    
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
    void dispatchChunk(Rc<DxvkCsChunk>&& chunk);
    
    /**
     * \brief Synchronizes with the thread
     * 
     * This waits for all chunks in the dispatch
     * queue to be processed by the thread. Note
     * that this does \e not implicitly call
     * \ref flush.
     */
    void synchronize();
    
  private:
    
    const Rc<DxvkContext>       m_context;
    
    std::atomic<bool>           m_stopped = { false };
    std::mutex                  m_mutex;
    std::condition_variable     m_condOnAdd;
    std::condition_variable     m_condOnSync;
    std::queue<Rc<DxvkCsChunk>> m_chunks;
    std::thread                 m_thread;
    
    void threadFunc();
    
  };
  
}