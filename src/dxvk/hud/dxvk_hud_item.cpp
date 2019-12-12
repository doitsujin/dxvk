#include "dxvk_hud_item.h"

#include <version.h>

namespace dxvk::hud {

  HudItem::~HudItem() {

  }


  void HudItem::update(dxvk::high_resolution_clock::time_point time) {
    // Do nothing by default. Some items won't need this.
  }


  HudItemSet::HudItemSet() {
    std::string configStr = env::getEnvVar("DXVK_HUD");

    if (configStr == "full") {
      // Just enable everything
      m_enableFull = true;
    } else if (configStr == "1") {
      m_enabled.insert("devinfo");
      m_enabled.insert("fps");
    } else {
      std::string::size_type pos = 0;
      std::string::size_type end = 0;
      
      while (pos < configStr.size()) {
        end = configStr.find(',', pos);
        
        if (end == std::string::npos)
          end = configStr.size();
        
        m_enabled.insert(configStr.substr(pos, end - pos));
        pos = end + 1;
      }
    }
  }


  HudItemSet::~HudItemSet() {

  }


  void HudItemSet::update() {
    auto time = dxvk::high_resolution_clock::now();

    for (const auto& item : m_items)
      item->update(time);
  }


  void HudItemSet::render(HudRenderer& renderer) {
    HudPos position = { 8.0f, 8.0f };

    for (const auto& item : m_items)
      position = item->render(renderer, position);
  }


  HudPos HudVersionItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      "DXVK " DXVK_VERSION);

    position.y += 8.0f;
    return position;
  }


  HudClientApiItem::HudClientApiItem(const Rc<DxvkDevice>& device)
  : m_device(device) {

  }


  HudClientApiItem::~HudClientApiItem() {

  }


  HudPos HudClientApiItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_device->clientApi());

    position.y += 8.0f;
    return position;
  }

}
