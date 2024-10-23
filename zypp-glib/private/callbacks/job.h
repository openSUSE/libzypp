/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_GLIB_ZMART_JOB_CALLBACKS_H
#define ZYPP_GLIB_ZMART_JOB_CALLBACKS_H

#include <iostream>

#include <zypp/base/Logger.h>
#include <zypp/base/String.h>
#include <zypp/ZYppCallbacks.h>

#include <zypp-core/zyppng/ui/userrequest.h>
#include <zypp/ng/reporthelper.h>
#include <zypp/ng/context.h>


namespace zyppng
{
  /**
   * \class JobReportReceiver
   * \brief Receive generic notification callbacks
   */
  struct JobReportReceiver : public zypp::callback::ReceiveReport<zypp::JobReport>
  {
    JobReportReceiver( SyncContextRef ctx ) : _ctx( std::move(ctx)) {
      if ( !ctx ) ZYPP_THROW( zypp::Exception("Context can not be null for JobReportReceiver") );
    }

    bool message( MsgType type_r, const std::string & msg_r, const UserData & userData_r ) const override
    {
      JobReportHelper<SyncContextRef, NewStyleReportTag> helper( _ctx );
      switch ( type_r.asEnum() )
      {
        case MsgType::debug:
          return helper.debug ( msg_r, userData_r );
        case MsgType::info:
          return helper.info ( msg_r, userData_r );
        case MsgType::warning:
          return helper.warning ( msg_r, userData_r );
        case MsgType::error:
          return helper.error ( msg_r, userData_r );
        case MsgType::important:
          return helper.important ( msg_r, userData_r );
        case MsgType::data:
          return helper.data ( msg_r, userData_r );
      }
      return true;
    }

  private:
    SyncContextRef _ctx;
  };
} // namespace zyppng

#endif // ZYPP_GLIB_ZMART_JOB_CALLBACKS_H
