#ifndef TESTTOOLS_H
#define TESTTOOLS_H

#include <string>
#include <zypp-core/Pathname.h>

namespace TestTools {
  // read all contents of a file into a string)
  std::string readFile ( const zypp::Pathname &file );

  // write all data as content into a new file
  bool writeFile ( const zypp::Pathname &file, const std::string &data );

  bool checkLocalPort( int portno_r );
}


#endif // TESTTOOLS_H
