SET( downloader_HEADERS
  downloader/private/downloader_p.h
  downloader/private/downloaderstates/base_p.h
  downloader/private/downloaderstates/basicdownloader_p.h
  downloader/private/downloaderstates/detectmeta_p.h
  downloader/private/downloaderstates/final_p.h
  downloader/private/downloaderstates/initial_p.h
  downloader/private/downloaderstates/metalinkinfo_p.h
  downloader/private/downloaderstates/metalink_p.h
  downloader/private/downloaderstates/mirrorhandling_p.h
  downloader/private/downloaderstates/normal_p.h
  downloader/private/downloaderstates/preparemulti_p.h
  downloader/private/downloaderstates/rangedownloader_p.h
  downloader/private/downloaderstates/zck_p.h
  downloader/private/mirrorcontrol_p.h
  downloader/downloader.h
  downloader/downloadspec.h
)


SET( downloader_SRCS
  downloader/private/downloaderstates/basicdownloader_p.cc
  downloader/private/downloaderstates/detectmeta_p.cc
  downloader/private/downloaderstates/final_p.cc
  downloader/private/downloaderstates/initial_p.cc
  downloader/private/downloaderstates/metalinkinfo_p.cc
  downloader/private/downloaderstates/metalink_p.cc
  downloader/private/downloaderstates/mirrorhandling_p.cc
  downloader/private/downloaderstates/normal_p.cc
  downloader/private/downloaderstates/preparemulti_p.cc
  downloader/private/downloaderstates/rangedownloader_p.cc
  downloader/private/downloaderstates/zck_p.cc

  downloader/mirrorcontrol.cc
  downloader/downloader.cc
  downloader/downloadspec.cc
)
