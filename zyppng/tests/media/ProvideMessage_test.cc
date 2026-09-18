#include <zypp-media/ng/private/providemessage_p.h>
#include <zypp-core/rpc/PluginFrame.h>
#include <boost/test/unit_test.hpp>
#include <string_view>

using namespace zyppng;
using namespace std::string_view_literals;

// Helper to perform round-trip: Factory -> STOMP -> Parser
static expected<ProvideMessage> roundTrip(const ProvideMessage& msg) {
    auto frame = msg.toStompMessage();
    if (!frame) return expected<ProvideMessage>::error(frame.error());
    return ProvideMessage::create(*frame);
}

BOOST_AUTO_TEST_CASE( test_provide_started_roundtrip )
{
    auto original = ProvideMessage::createProvideStarted(123, zypp::Url("http://test.com"), "/tmp/local", "/tmp/staging");
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->value(ProvideStartedMsgFields::Url).asString(), "http://test.com/");
    BOOST_CHECK_EQUAL(parsed->value(ProvideStartedMsgFields::LocalFilename).asString(), "/tmp/local");
}

BOOST_AUTO_TEST_CASE( test_provide_finished_roundtrip )
{
    auto original = ProvideMessage::createProvideFinished(1, "/tmp/file", true);
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->value(ProvideFinishedMsgFields::LocalFilename).asString(), "/tmp/file");
    BOOST_CHECK_EQUAL(parsed->value(ProvideFinishedMsgFields::CacheHit).asBool(), true);
}

BOOST_AUTO_TEST_CASE( test_attach_finished_roundtrip )
{
    auto original = ProvideMessage::createAttachFinished(1, "/mnt/mountpoint");
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->value(AttachFinishedMsgFields::LocalMountPoint).asString(), "/mnt/mountpoint");
}

BOOST_AUTO_TEST_CASE( test_detach_finished_roundtrip )
{
    auto original = ProvideMessage::createDetachFinished(1);
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->code(), ProvideMessage::Code::DetachFinished);
}

BOOST_AUTO_TEST_CASE( test_auth_info_roundtrip )
{
    std::map<std::string, std::string> extras = {{"hint", "basic"}};
    auto original = ProvideMessage::createAuthInfo(1, "user", "pass", 123456789, extras);
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->value(AuthInfoMsgFields::Username).asString(), "user");
    BOOST_CHECK_EQUAL(parsed->value(AuthInfoMsgFields::AuthTimestamp).asInt64(), 123456789);
    BOOST_CHECK_EQUAL(parsed->value("hint"sv).asString(), "basic");
}

BOOST_AUTO_TEST_CASE( test_media_changed_roundtrip )
{
    auto original = ProvideMessage::createMediaChanged(1);
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->code(), ProvideMessage::Code::MediaChanged);
}

BOOST_AUTO_TEST_CASE( test_redirect_roundtrip )
{
    auto original = ProvideMessage::createRedirect(1, zypp::Url("http://mirror.com"));
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->value(RedirectMsgFields::NewUrl).asString(), "http://mirror.com/");
}

BOOST_AUTO_TEST_CASE( test_provide_request_roundtrip )
{
    auto original = ProvideMessage::createProvide(1, zypp::Url("http://test.com"), "file.rpm", "delta.rpm", 1024, true);
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->value(ProvideMsgFields::Url).asString(), "http://test.com/");
    BOOST_CHECK_EQUAL(parsed->value(ProvideMsgFields::Filename).asString(), "file.rpm");
    BOOST_CHECK_EQUAL(parsed->value(ProvideMsgFields::ExpectedFilesize).asInt64(), 1024);
    BOOST_CHECK_EQUAL(parsed->value(ProvideMsgFields::CheckExistOnly).asBool(), true);
}

BOOST_AUTO_TEST_CASE( test_cancel_roundtrip )
{
    auto original = ProvideMessage::createCancel(1);
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->code(), ProvideMessage::Code::Cancel);
}

BOOST_AUTO_TEST_CASE( test_attach_request_roundtrip )
{
    auto original = ProvideMessage::createAttach(1, zypp::Url("dvd:/"), "uuid", "Label", "suseV1", "data", 1);
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->value(AttachMsgFields::AttachId).asString(), "uuid");
    BOOST_CHECK_EQUAL(parsed->value(AttachMsgFields::VerifyType).asString(), "suseV1");
}

BOOST_AUTO_TEST_CASE( test_detach_request_roundtrip )
{
    auto original = ProvideMessage::createDetach(1, zypp::Url("dvd-media://uuid/"));
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->value(DetachMsgFields::Url).asString(), "dvd-media://uuid/");
}

BOOST_AUTO_TEST_CASE( test_auth_data_request_roundtrip )
{
    std::map<std::string, std::string> extras = {{"realm", "secure"}};
    auto original = ProvideMessage::createAuthDataRequest(1, zypp::Url("http://host.com"), "lastuser", 999, extras);
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->value(AuthDataRequestMsgFields::EffectiveUrl).asString(), "http://host.com/");
    BOOST_CHECK_EQUAL(parsed->value(AuthDataRequestMsgFields::LastUser).asString(), "lastuser");
    BOOST_CHECK_EQUAL(parsed->value(AuthDataRequestMsgFields::LastAuthTimestamp).asInt64(), 999);
    auto val = parsed->value(std::string("realm"));
    BOOST_CHECK_EQUAL(val.asString(), "secure");
}

BOOST_AUTO_TEST_CASE( test_media_change_request_roundtrip )
{
    std::vector<std::string> devs = {"/dev/sr0", "/dev/sr1"};
    auto original = ProvideMessage::createMediaChangeRequest(1, "SLES", 2, devs, "Insert DVD 2");
    auto parsed = roundTrip(original);
    BOOST_REQUIRE(parsed);
    BOOST_CHECK_EQUAL(parsed->value(MediaChangeRequestMsgFields::Label).asString(), "SLES");
    BOOST_CHECK_EQUAL(parsed->value(MediaChangeRequestMsgFields::MediaNr).asInt(), 2);
    BOOST_CHECK_EQUAL(parsed->value(MediaChangeRequestMsgFields::Desc).asString(), "Insert DVD 2");

    // Verify multi-value headers (devices)
    auto values = parsed->values(MediaChangeRequestMsgFields::Device);
    BOOST_REQUIRE_EQUAL(values.size(), 2);
    BOOST_CHECK_EQUAL(values[0].asString(), "/dev/sr0");
    BOOST_CHECK_EQUAL(values[1].asString(), "/dev/sr1");
}

BOOST_AUTO_TEST_CASE( test_error_responses )
{
    // Test a subset of error codes to verify grouping
    std::vector<ProvideMessage::Code> codes = {
        ProvideMessage::Code::BadRequest,
        ProvideMessage::Code::Forbidden,
        ProvideMessage::Code::InternalError
    };

    for (auto code : codes) {
        auto original = ProvideMessage::createErrorResponse(1, code, "Fail", true);
        auto parsed = roundTrip(original);
        BOOST_REQUIRE(parsed);
        BOOST_CHECK_EQUAL(parsed->code(), code);
        BOOST_CHECK_EQUAL(parsed->value(ErrMsgFields::Reason).asString(), "Fail");
    }
}

BOOST_AUTO_TEST_CASE( test_malformed_parsing )
{
    // Invalid int
    {
        zypp::PluginFrame frame((std::string(ProvideMessage::typeName)));
        frame.addHeader(std::string(ProvideMessageFields::RequestCode), "202");
        frame.addHeader(std::string(ProvideMessageFields::RequestId), "1");
        frame.addHeader(std::string(AuthInfoMsgFields::Username), "u");
        frame.addHeader(std::string(AuthInfoMsgFields::Password), "p");
        frame.addHeader(std::string(AuthInfoMsgFields::AuthTimestamp), "nan");
        BOOST_CHECK(!ProvideMessage::create(frame));
    }

    // Invalid bool
    {
        zypp::PluginFrame frame((std::string(ProvideMessage::typeName)));
        frame.addHeader(std::string(ProvideMessageFields::RequestCode), "200");
        frame.addHeader(std::string(ProvideMessageFields::RequestId), "1");
        frame.addHeader(std::string(ProvideFinishedMsgFields::LocalFilename), "f");
        frame.addHeader(std::string(ProvideFinishedMsgFields::CacheHit), "maybe");
        BOOST_CHECK(!ProvideMessage::create(frame));
    }
}
