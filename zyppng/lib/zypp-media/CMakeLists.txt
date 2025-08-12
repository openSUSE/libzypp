SET( zypp_media_ng_HEADERS
  ng/provide.h
  ng/Provide
  ng/provide-configvars.h
  ng/provideres.h
  ng/provideitem.h
  ng/ProvideRes
  ng/mediaverifier.h
  ng/MediaVerifier
  ng/worker/devicedriver.h
  ng/worker/DeviceDriver
  ng/worker/provideworker.h
  ng/worker/ProvideWorker
  ng/worker/mountingworker.h
  ng/worker/MountingWorker
)

SET( zypp_media_ng_private_HEADERS
  ng/private/attachedmediainfo_p.h
  ng/private/provide_p.h
  ng/private/providefwd_p.h
  ng/private/provideitem_p.h
  ng/private/providemessage_p.h
  ng/private/providequeue_p.h
  ng/private/provideres_p.h
  ng/private/providedbg_p.h
)

SET( zypp_media_ng_SRCS
  ng/attachedmediainfo.cc
  ng/provide.cc
  ng/provideres.cc
  ng/provideitem.cc
  ng/providemessage.cc
  ng/providequeue.cc
  ng/mediaverifier.cc
  ng/worker/devicedriver.cc
  ng/worker/provideworker.cc
  ng/worker/mountingworker.cc
)

include(${zypp-libs_SOURCE_DIR}/zypp-logic/zypp-media/zypp-media.cmake)
zypp_add_media_target( TARGETNAME "zyppng-media" HEADERS ${zypp_media_ng_HEADERS} ${zypp_media_ng_private_HEADERS} SOURCES ${zypp_media_ng_SRCS} )

