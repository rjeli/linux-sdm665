// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/of.h>

#include <drm/drm_device.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct tianma_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;
	int enabled;
	int prepared;
};

static inline struct tianma_panel *to_tp(struct drm_panel *panel)
{
	return container_of(panel, struct tianma_panel, base);
}

#define tp_info(...) dev_info(&tp->dsi->dev, ##__VA_ARGS__)

static int tianma_nt36672a_panel_enable(struct drm_panel *panel)
{
	struct tianma_panel *tp = to_tp(panel);
	tp_info("panel_enable\n");
	if (tp->enabled) {
		tp_info("already enabled\n");
		return 0;
	}
	tp->enabled = 1;
	return 0;
}

static int tianma_nt36672a_panel_disable(struct drm_panel *panel)
{
	struct tianma_panel *tp = to_tp(panel);
	tp_info("panel_disable\n");
	if (!tp->enabled) {
		tp_info("wasn't enabled\n");
		return 0;
	}
	tp->enabled = 0;
	return 0;
}

/* ripped from downstream dtsi */
static u8 dsi_on_cmd_data[] = {
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x25,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xfb, 0x01,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x18, 0x96,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x05, 0x04,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x20,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xfb, 0x01,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x78, 0x01,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x24,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xfb, 0x01,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x82, 0x13,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x84, 0x31,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x88, 0x13,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x8a, 0x31,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x8e, 0xe4,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x8f, 0x01,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x90, 0x80,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x26,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xfb, 0x01,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xa9, 0x12,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xaa, 0x10,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xae, 0x8a,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x10,
	0x05, 0x01, 0x00, 0x00, 0x50, 0x00, 0x02, 0x11, 0x00,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0xb0, 0x01,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x35, 0x00,
	0x39, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03, 0x68, 0x04, 0x03,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x51, 0xB8,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x53, 0x2c,
	0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x55, 0x00,
	0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x29, 0x00,
};

static u8 dsi_off_cmd_data[] = {
	0x05, 0x01, 0x00, 0x00, 0x14, 0x00, 0x02, 0x28, 0x00,
	0x05, 0x01, 0x00, 0x00, 0x78, 0x00, 0x02, 0x10, 0x00,
};

static int dsi_panel_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt)
{
	const u32 cmd_set_min_size = 7;
	u32 count = 0;
	u32 packet_length;
	u32 tmp;

	while (length >= cmd_set_min_size) {
		packet_length = cmd_set_min_size;
		tmp = ((data[5] << 8) | (data[6]));
		packet_length += tmp;
		if (packet_length > length) {
			pr_err("format error\n");
			return -EINVAL;
		}
		length -= packet_length;
		data += packet_length;
		count++;
	};

	*cnt = count;
	return 0;
}

static int tianma_nt36672a_panel_prepare(struct drm_panel *panel)
{
	struct tianma_panel *tp = to_tp(panel);
	int pkt_cnt;
	int ret;

	tp_info("panel_prepare\n");
	if (tp->prepared) {
		tp_info("already prepared\n");
		return 0;
	}

	/* for now, skip regulators and gpios, assume the bl set them up */

	ret = dsi_panel_get_cmd_pkt_count(
		dsi_on_cmd_data, ARRAY_SIZE(dsi_on_cmd_data), &pkt_cnt);
	if (ret) {
		tp_info("unable to get dsi_on cmd_pkt_count: %d\n", ret);
		return -EINVAL;
	}

	tp_info("got %d on packets\n", pkt_cnt);

	ret = dsi_panel_get_cmd_pkt_count(
		dsi_off_cmd_data, ARRAY_SIZE(dsi_off_cmd_data), &pkt_cnt);
	if (ret) {
		tp_info("unable to get dsi_off cmd_pkt_count: %d\n", ret);
		return -EINVAL;
	}

	tp_info("got %d off packets\n", pkt_cnt);

	tp->prepared = 1;
	return 0;
}

static int tianma_nt36672a_panel_unprepare(struct drm_panel *panel)
{
	struct tianma_panel *tp = to_tp(panel);
	tp_info("panel_unprepare\n");
	if (!tp->prepared) {
		tp_info("wasn't prepared");
	}
	tp->prepared = 0;
	return 0;
}

static const int H_ACTIVE = 1080;
static const int H_FRONT_PORCH = 90;
static const int H_SYNC_PULSE = 2;
static const int H_BACK_PORCH = 120;

static const int V_ACTIVE = 2340;
static const int V_FRONT_PORCH = 10;
static const int V_SYNC_PULSE = 3;
static const int V_BACK_PORCH = 8;

static int tianma_nt36672a_panel_get_modes(struct drm_panel *panel,
					   struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode default_mode = {0};
	int pos;

	dev_info(panel->dev, "getting modes\n");

#define NEXT_POS(member, width) do { pos += width; default_mode.member = pos; } while (0)
	pos = 0;
	NEXT_POS(hdisplay, H_ACTIVE);
	NEXT_POS(hsync_start, H_FRONT_PORCH);
	NEXT_POS(hsync_end, H_SYNC_PULSE);
	NEXT_POS(htotal, H_BACK_PORCH);
	pos = 0;
	NEXT_POS(vdisplay, V_ACTIVE);
	NEXT_POS(vsync_start, V_FRONT_PORCH);
	NEXT_POS(vsync_end, V_SYNC_PULSE);
	NEXT_POS(vtotal, V_BACK_PORCH);
#undef NEXT_POS

	default_mode.vrefresh = 60;
	/* see dsi_panel.c:2591
	 * h_total * v_total * refresh_rate / 1000
	 * or not, if dsc is being used
	 */
	default_mode.clock = 183025,
	default_mode.flags = 0;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 67;
	connector->display_info.height_mm = 145;

	return 1;
}

static const struct drm_panel_funcs tianma_nt36672a_panel_funcs = {
	.enable = tianma_nt36672a_panel_enable,
	.disable = tianma_nt36672a_panel_disable,
	.prepare = tianma_nt36672a_panel_prepare,
	.unprepare = tianma_nt36672a_panel_unprepare,
	.get_modes = tianma_nt36672a_panel_get_modes,
};

static int tianma_nt36672a_panel_add(struct tianma_panel *tp)
{
	tp_info("panel_add\n");

	drm_panel_init(&tp->base, &tp->dsi->dev,
		&tianma_nt36672a_panel_funcs, DRM_MODE_CONNECTOR_DSI);

	return drm_panel_add(&tp->base);
}

static int tianma_nt36672a_probe(struct mipi_dsi_device *dsi)
{
	struct tianma_panel *tp;
	int ret;

	dev_info(&dsi->dev, "========= PROBING ========\n");

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO;

	tp = devm_kzalloc(&dsi->dev, sizeof(*tp), GFP_KERNEL);
	if (!tp)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, tp);
	tp->dsi = dsi;

	ret = tianma_nt36672a_panel_add(tp);
	if (ret < 0)
		return ret;

	return mipi_dsi_attach(dsi);
}

static int tianma_nt36672a_remove(struct mipi_dsi_device *dsi)
{
	struct tianma_panel *tp = mipi_dsi_get_drvdata(dsi);
	int ret;

	dev_info(&dsi->dev, "removing\n");

	ret = drm_panel_disable(&tp->base);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to disable panel: %d\n", ret);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	if (tp->base.dev)
		drm_panel_remove(&tp->base);

	return 0;
}

static const struct of_device_id tianma_nt36672a_of_match[] = {
	{ .compatible = "tianma,nt36672a-display", },
	{ }
};
MODULE_DEVICE_TABLE(of, tianma_nt36672a_of_match);

static struct mipi_dsi_driver tianma_nt36672a_driver = {
	.driver = {
		.name = "panel-tianma-nt36672a",
		.of_match_table = tianma_nt36672a_of_match,
	},
	.probe = tianma_nt36672a_probe,
	.remove = tianma_nt36672a_remove,
};
module_mipi_dsi_driver(tianma_nt36672a_driver);

MODULE_DESCRIPTION("Tianma NT36672A DSI Panel Driver");
MODULE_LICENSE("GPL v2");