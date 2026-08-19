include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI Sub Converter
LUCI_DESCRIPTION:=Web interface for the sub-converter daemon
LUCI_DEPENDS:=+luci-base +sub-converter
LUCI_PKGARCH:=all

PKG_VERSION:=0.1.0
PKG_RELEASE:=1

include $(TOPDIR)/feeds/luci/luci.mk

# gh-action-sdk may skip building the daemon in its per-package loop;
# make sure it is always built first so this UI package can be installed.
define Build/Compile
	$(MAKE) $(MAKE_FLAGS) package/sub-converter/compile
endef

# call BuildPackage - OpenWrt buildroot signature
