#include "TestTools.h"

#include <zypp-core/AutoDispose.h>
#include <zypp-core/fs/PathInfo.h>

#include <iostream>
#include <fstream>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

namespace TestTools {

  std::string readFile(const zypp::Pathname &file)
  {
    if ( ! zypp::PathInfo( file ).isFile() ) {
      return std::string();
    }
    std::ifstream istr( file.asString().c_str() );
    if ( ! istr ) {
      return std::string();
    }

    std::string str((std::istreambuf_iterator<char>(istr)),
                    std::istreambuf_iterator<char>());
    return str;
  }

  bool checkLocalPort( int portno_r )
  {
    bool ret = false;

    zypp::AutoDispose<int> sockfd { socket( AF_INET, SOCK_STREAM, 0 ) };
    if ( sockfd < 0 ) {
        std::cerr << "ERROR opening socket" << std::endl;
        return ret;
    }
    sockfd.setDispose( ::close );

    struct hostent * server = gethostbyname( "127.0.0.1" );
    if ( server == nullptr ) {
        std::cerr << "ERROR, no such host 127.0.0.1" << std::endl;
        return ret;
    }

    struct sockaddr_in serv_addr;
    bzero( &serv_addr, sizeof(serv_addr) );
    serv_addr.sin_family = AF_INET;
    bcopy( server->h_addr, &serv_addr.sin_addr.s_addr, server->h_length );
    serv_addr.sin_port = htons(portno_r);

    return( connect( sockfd, (const sockaddr*)&serv_addr, sizeof(serv_addr) ) == 0 );
  }

  bool writeFile( const zypp::Pathname &fileName, const std::string &data )
  {
    std::ofstream file;
    file.open( fileName.c_str() );
    if ( file.fail() ) {
      std::cerr << "Failed to create file " << fileName << std::endl;
      return false;
    }
    file << data;
    file.close();
    return true;
  }

}
