#ifndef ZYPPNG_IO_PRIVATE_FORKSPAWNENGINE_H
#define ZYPPNG_IO_PRIVATE_FORKSPAWNENGINE_H

#include "abstractspawnengine_p.h"
#include <glib.h>

namespace zyppng {

  class AbstractDirectSpawnEngine : public AbstractSpawnEngine
  {
  public:
    ~AbstractDirectSpawnEngine();
    virtual bool isRunning ( bool wait = false ) override;

  protected:
    void mapExtraFds( int controlFd = -1 );
    void resetSignals();
  };

  /*!
    \internal
    Process forking engine that's using the traditional fork() approach
   */
  class ForkSpawnEngine : public AbstractDirectSpawnEngine
  {
  public:
    bool start( const char *const *argv, int stdin_fd, int stdout_fd, int stderr_fd  ) override;
    bool usePty () const;
    void setUsePty ( const bool set = true );

  private:
    /**
     * Set to true, if a pair of ttys is used for communication
     * instead of a pair of pipes.
     */
    bool _use_pty = false;
  };

#if GLIB_CHECK_VERSION( 2, 58, 0)

#define ZYPP_HAS_GLIBSPAWNENGINE 1

  /*!
    \internal
    Process forking engine that's using g_spawn from glib which can in most cases optimize
    using posix_spawn.
   */
  class GlibSpawnEngine : public AbstractDirectSpawnEngine
  {
  public:
    bool start( const char *const *argv, int stdin_fd, int stdout_fd, int stderr_fd  ) override;

  private:
    static void glibSpawnCallback ( void *data );
  };

#else
  #define ZYPP_HAS_GLIBSPAWNENGINE 0
#endif

}


#endif //
