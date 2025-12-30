include $(TOPDIR)/rules.mk

# 模块基础信息（按需改名字）
PKG_NAME:=script
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

# 包的分类和描述（menuconfig里能找到）
define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=My Simple Script
  DESCRIPTION:=A minimal script module for OpenWRT
endef

# 编译阶段：脚本无需编译，直接留空
define Build/Compile
endef

# 安装阶段：直接读取仓库根目录的脚本，复制到设备
define Package/$(PKG_NAME)/install
	
	$(INSTALL_DIR) $(1)/usr/bin
	
	$(INSTALL_BIN) $(CURDIR)/script.sh $(1)/usr/bin/
endef

# 固定构建指令
$(eval $(call BuildPackage,$(PKG_NAME)))