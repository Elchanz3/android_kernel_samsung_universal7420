dtb-$(CONFIG_BOARD_ZEROFLTE) += exynos7420-zeroflte_eur_open_00.dtb \
 exynos7420-zeroflte_eur_open_01.dtb \
 exynos7420-zeroflte_eur_open_02.dtb \
 exynos7420-zeroflte_eur_open_03.dtb \
 exynos7420-zeroflte_eur_open_04.dtb \
 exynos7420-zeroflte_eur_open_05.dtb \
 exynos7420-zeroflte_eur_open_06.dtb \
 exynos7420-zeroflte_eur_open_07.dtb
dtb-$(CONFIG_BOARD_ZEROLTE) += exynos7420-zerolte_eur_open_01.dtb \
 exynos7420-zerolte_eur_open_02.dtb \
 exynos7420-zerolte_eur_open_03.dtb \
 exynos7420-zerolte_eur_open_04.dtb \
 exynos7420-zerolte_eur_open_05.dtb \
 exynos7420-zerolte_eur_open_06.dtb \
 exynos7420-zerolte_eur_open_07.dtb \
 exynos7420-zerolte_eur_open_08.dtb
dtb-$(CONFIG_BOARD_ZEROFLTECAN) += exynos7420-zeroflte_usa_00.dtb \
 exynos7420-zeroflte_usa_01.dtb \
 exynos7420-zeroflte_usa_02.dtb \
 exynos7420-zeroflte_usa_03.dtb \
 exynos7420-zeroflte_usa_04.dtb \
 exynos7420-zeroflte_usa_05.dtb
dtb-$(CONFIG_BOARD_ZEROLTECAN) += exynos7420-zerolte_usa_01.dtb \
 exynos7420-zerolte_usa_02.dtb \
 exynos7420-zerolte_usa_03.dtb \
 exynos7420-zerolte_usa_04.dtb \
 exynos7420-zerolte_usa_05.dtb \
 exynos7420-zerolte_usa_06.dtb \
 exynos7420-zerolte_usa_07.dtb
dtb-$(CONFIG_BOARD_ZENLTE) += exynos7420-zenlte_eur_open_00.dtb  \
 exynos7420-zenlte_eur_open_01.dtb \
 exynos7420-zenlte_eur_open_02.dtb \
 exynos7420-zenlte_eur_open_03.dtb \
 exynos7420-zenlte_eur_open_04.dtb \
 exynos7420-zenlte_eur_open_07.dtb \
 exynos7420-zenlte_eur_open_08.dtb \
 exynos7420-zenlte_eur_open_09.dtb
dtb-$(CONFIG_BOARD_NOBLELTE) += exynos7420-noblelte_eur_open_00.dtb \
 exynos7420-noblelte_eur_open_01.dtb \
 exynos7420-noblelte_eur_open_02.dtb \
 exynos7420-noblelte_eur_open_03.dtb \
 exynos7420-noblelte_eur_open_04.dtb \
 exynos7420-noblelte_eur_open_05.dtb \
 exynos7420-noblelte_eur_open_06.dtb \
 exynos7420-noblelte_eur_open_08.dtb \
 exynos7420-noblelte_eur_open_09.dtb
dtb-$(CONFIG_BOARD_ZENLTECAN) += exynos7420-zenlte_usa_00.dtb \
 exynos7420-zenlte_usa_01.dtb \
 exynos7420-zenlte_usa_02.dtb \
 exynos7420-zenlte_usa_03.dtb \
 exynos7420-zenlte_usa_04.dtb \
 exynos7420-zenlte_usa_08.dtb \
 exynos7420-zenlte_usa_09.dtb
dtb-$(CONFIG_BOARD_NOBLELTECAN) += exynos7420-noblelte_usa_00.dtb  \
 exynos7420-noblelte_usa_01.dtb \
 exynos7420-noblelte_usa_02.dtb \
 exynos7420-noblelte_usa_03.dtb \
 exynos7420-noblelte_usa_04.dtb \
 exynos7420-noblelte_usa_06.dtb \
 exynos7420-noblelte_usa_07.dtb \
 exynos7420-noblelte_usa_08.dtb \
 exynos7420-noblelte_usa_09.dtb
	
targets += dtbs

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif
targets += $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := dts/*.dtb *.dtb
