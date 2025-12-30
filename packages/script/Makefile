include $(TOPDIR)/rules.mk

PKG_NAME:=script
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

# 包的基础信息（去掉DESCRIPTION行）
define Package/$(PKG_NAME)
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=My Simple Script
endef

# 新的描述语法（OpenWRT推荐写法）
define Package/$(PKG_NAME)/description
A minimal script module for OpenWRT
endef

# 编译阶段（脚本无需编译）
define Build/Compile
endef

# 安装阶段
define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(CURDIR)/script.sh $(1)/usr/bin/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))

# 最后保留一个空行