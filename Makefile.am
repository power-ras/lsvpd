update_h_files = src/include/device.hpp \
		src/include/devicelookup.hpp \
		src/include/fswalk.hpp \
		src/include/gatherer.hpp \
		src/include/icollector.hpp \
		src/include/manufacturer.hpp \
		src/include/proccollector.hpp \
		src/include/devicetreecollector.hpp \
		src/include/subdevice.hpp \
		src/include/rtascollector.hpp \
		src/include/sysfstreecollector.hpp

INCLUDES = -I./src/include

sbin_PROGRAMS = vpdupdate lsvpd lscfg lsvio lsmcode invscout

vpdupdate_SOURCES = src/internal/sys_interface/devicetreecollector.cpp \
		src/internal/sys_interface/fswalk.cpp \
		src/internal/updater.cpp \
		src/common/lsvpd_error_handler.cpp \
		src/internal/subdevice.cpp \
		src/internal/device.cpp \
		src/internal/manufacturer.cpp \
		src/internal/devicelookup.cpp \
		src/internal/sys_interface/gatherer.cpp \
		src/internal/sys_interface/sysfstreecollector.cpp \
		src/internal/sys_interface/proccollector.cpp \
		src/internal/sys_interface/icollector.cpp \
		src/internal/sys_interface/sysfs_SCSI_Fill.cpp \
		src/internal/sys_interface/rtascollector.cpp \
		$(update_h_files)
		
lsvpd_SOURCES = src/output/lsvpd.cpp \
		$(update_h_files)
		
lscfg_SOURCES = src/output/lscfg.cpp \
		$(update_h_files)

lsvio_SOURCES = src/output/lsvio.cpp \
		$(update_h_files)
		
lsmcode_SOURCES = src/output/lsmcode.cpp \
		  src/internal/sys_interface/rtascollector.cpp \
		$(update_h_files)

invscout_SOURCES = src/output/invscout.cpp \
		$(update_h_files)

vpdupdate_LDADD = -lz -lstdc++ -lvpd_cxx
if SGUTIL1
vpdupdate_LDADD += -lsgutils
endif

if SGUTIL2
vpdupdate_LDADD += -lsgutils2
endif

vpdupdate_LDFLAGS = -Wall
lsvpd_LDADD = -lz -lstdc++ -lvpd_cxx
lsvpd_LDFLAGS = -Wall
lscfg_LDADD = -lz -lstdc++ -lvpd_cxx
lscfg_LDFLAGS = -Wall
lsvio_LDADD = -lz -lstdc++ -lvpd_cxx
lsvio_LDFLAGS = -Wall
lsmcode_LDADD = -lz -lstdc++ -lvpd_cxx
lsmcode_LDFLAGS = -Wall
invscout_LDADD = -lz -lstdc++ -lvpd_cxx
invscout_LDFLAGS = -Wall

AM_CXXFLAGS = -DDEST_DIR='"${exec_prefix}"' -DVPD_VERSION='"@VERSION@"'

if PCI_IDS_LOC
AM_CXXFLAGS += -DPCI_IDS='"/usr/share/pci.ids"'
else
if PCI_IDS_LOC2
AM_CXXFLAGS += -DPCI_IDS='"/usr/share/hwdata/pci.ids"'
else
AM_CXXFLAGS += -DPCI_IDS='"/usr/share/misc/pci.ids"'
endif
endif

if USB_IDS_LOC
AM_CXXFLAGS += -DUSB_IDS='"/usr/share/usb.ids"'
else
if USB_IDS_LOC2
AM_CXXFLAGS += -DUSB_IDS='"/usr/share/hwdata/usb.ids"'
else
AM_CXXFLAGS += -DUSB_IDS='"/usr/share/misc/usb.ids"'
endif
endif

SUBDIRS = docs

install-exec-hook:
	install -D --mode=644 scsi_templates.conf \
		${DESTDIR}/etc/lsvpd/scsi_templates.conf
	install -D --mode=644 cpu_mod_conv.conf \
		${DESTDIR}/etc/lsvpd/cpu_mod_conv.conf

EXTRA_DIST = docs/lsvpd.8 docs/lscfg.8 docs/lsvio.8 docs/lsmcode.8 docs/invscout.8 \
		docs/vpdupdate.8 docs/Makefile docs/install.html \
		bootstrap.sh scsi_templates.conf cpu_mod_conv.conf
