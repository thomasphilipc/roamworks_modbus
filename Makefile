include $(TOPDIR)/rules.mk
PKG_NAME:=modbus_rw
PKG_RELEASE:=0
PKG_VERSION:=1.2

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Roamworks Packages
  SUBMENU:=Bin utilities
  TITLE:= RW Modbus Master TCP
  DEPENDS:= +libpthread +libmysqlclient +librt +libmodbus_database +libcurl
endef
define Package/$(PKG_NAME)/description
 This is a Roamworks Modbus Master program
endef
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef
define Package/$(PKG_NAME)/Build/Configure
endef
define Package/$(PKG_NAME)/Build/Compile
 
endef


define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(PKG_NAME) $(1)/usr/bin/
	
endef
$(eval $(call BuildPackage,$(PKG_NAME)))
