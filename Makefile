include $(TOPDIR)/rules.mk

PKG_NAME:=minihttp
PKG_VERSION:=0.1
PKG_RELEASE:=1
PKG_MAINTAINER:=Chertov Maxim <chertovmv@gmail.com>

PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/$(PKG_NAME)
	SECTION:=multimedia
	CATEGORY:=Multimedia
	TITLE:=Some samples for Hisilicon devices
	MAINTAINER:=chertovmv@gmail.com
	DEPENDS:=+libpthread
endef

define Package/$(PKG_NAME)/description
	Some samples for Hisilicon devices
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./* $(PKG_BUILD_DIR)/
endef

# define Package/$(PKG_NAME)/install
# 	$(INSTALL_DIR) $(1)/usr/bin
# 	$(INSTALL_BIN) $(PKG_BUILD_DIR)/minihttp $(1)/usr/bin/
# endef

# define Package/install
# 	$(INSTALL_DIR) $(1)/usr/bin
# 	$(INSTALL_BIN) $(PKG_BUILD_DIR)/minihttp $(1)/usr/bin/
# endef

$(eval $(call BuildPackage,$(PKG_NAME)))
