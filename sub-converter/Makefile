include $(TOPDIR)/rules.mk

PKG_NAME:=sub-converter
PKG_VERSION:=0.1.0
PKG_RELEASE:=1

PKG_MAINTAINER:=Sub Converter Contributors
PKG_LICENSE:=GPL-2.0-or-later

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/sub-converter
	SECTION:=net
	CATEGORY:=Network
	TITLE:=Subscription converter daemon (sing-box/base64/clash -> mihomo)
	DEPENDS:=+libmicrohttpd +libcurl +libjson-c
endef

CMAKE_OPTIONS += -DBUILD_TESTING=OFF

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) $(CURDIR)/CMakeLists.txt $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/src $(PKG_BUILD_DIR)/
endef

define Package/sub-converter/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/subconvd $(1)/usr/sbin/subconvd
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) $(CURDIR)/files/etc/config/sub-converter $(1)/etc/config/sub-converter
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(CURDIR)/files/sub-converter.init $(1)/etc/init.d/sub-converter
endef

define Package/sub-converter/conffiles
/etc/config/sub-converter
endef

$(eval $(call BuildPackage,sub-converter))
