/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// Automatically generated wrapper for zypp/IdString.h
// Source Namespace: zypp -> Target Namespace: zyppng
#ifndef IDSTRING_H_ZYPPNG_WRAPPER_H
#define IDSTRING_H_ZYPPNG_WRAPPER_H

#include <zypp/IdString.h>

namespace zyppng
{
    using ::zypp::IdString;
    using IdStringSet = std::unordered_set<IdString>;
    using ::zypp::operator<<;
    using ::zypp::dumpOn;
    using ::zypp::operator==;
    using ::zypp::operator!=;
    using ::zypp::operator<;
    using ::zypp::operator<=;
    using ::zypp::operator>;
    using ::zypp::operator>=;
} // namespace zyppng
#endif // IDSTRING_H_ZYPPNG_WRAPPER_H
