include $(TOPDIR)/rules.mk

PKG_NAME:=mt6000-config-server
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=MT6000 Config Server
  DEPENDS:=+libpthread
endef

define Package/$(PKG_NAME)/description
  WiFi configuration server for MT6000 router
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	cd $(PKG_BUILD_DIR) && \
	$(TARGET_CC) $(TARGET_CFLAGS) -Wall -O2 -pthread \
		main.c server.c config_parser.c ssdp.c \
		-o mt6000-config-server \
		$(TARGET_LDFLAGS) -lpthread
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mt6000-config-server $(1)/usr/bin/
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/mt6000-config-server.init $(1)/etc/init.d/mt6000-config-server
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
