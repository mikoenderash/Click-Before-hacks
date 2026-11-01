#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <deque>
using namespace geode::prelude;

static int maxFrames() {
    auto m = Mod::get()->getSettingValue<std::string>("mode");
    if (m == "Pro (1 frame)") return 1;
    if (m == "Medio (2 frames)") return 2;
    if (m == "Custom") return (int)Mod::get()->getSettingValue<int64_t>("custom-frames");
    return 0;
}

// Texto "Nooo" arriba de la pantalla; sobrevive al cambio de escena
static void showNooo() {
    auto win = CCDirector::get()->getWinSize();
    auto label = CCLabelBMFont::create("Nooo", "bigFont.fnt");
    label->setPosition({ win.width / 2, win.height - 30.f });
    label->setScale(1.2f);
    label->setZOrder(10000);
    SceneManager::get()->keepAcrossScenes(label);
    label->runAction(CCSequence::create(
        CCDelayTime::create(2.5f),
        CCFadeOut::create(0.5f),
        CCCallFunc::create(label, callfunc_selector(CCNode::removeFromParent)),
        nullptr
    ));
}

class $modify(CFPlayLayer, PlayLayer) {
    struct Fields {
        std::deque<Ref<CheckpointObject>> history;
        bool replaying = false;
        int frame = 0;
        int lastClick = -1000;
        bool quitting = false;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto f = m_fields.self();
        if (f->replaying) return;
        f->frame++;

        // --- Salida forzada al 98% (no desactivable) ---
        if (!f->quitting && this->getCurrentPercent() >= 98.f) {
            f->quitting = true;
            showNooo();
            this->onQuit();
            return;
        }

        // --- Historial para el click fix ---
        if (maxFrames() == 0 || m_isPracticeMode) return;
        f->history.push_back(this->createCheckpoint());
        while (f->history.size() > 14) f->history.pop_front();
    }

    // Nunca se completa el nivel
    void levelComplete() {
        auto f = m_fields.self();
        if (f->quitting) return;
        f->quitting = true;
        showNooo();
        this->onQuit();
    }

    // Si mueres justo tras un click, prueba haberlo hecho k frames antes
    void destroyPlayer(PlayerObject* p, GameObject* obj) {
        auto f = m_fields.self();
        int max = maxFrames();
        if (f->replaying) return PlayLayer::destroyPlayer(p, obj);
        if (max == 0 || p != m_player1 || m_isPracticeMode
            || f->frame - f->lastClick > max + 1)
            return PlayLayer::destroyPlayer(p, obj);

        int sinceClick = f->frame - f->lastClick;
        float dt = 1.f / 240.f; // ajusta a tu TPS
        f->replaying = true;
        for (int k = 1; k <= max; k++) {
            int idx = (int)f->history.size() - 1 - sinceClick - k;
            if (idx < 0) break;
            this->loadFromCheckpoint(f->history[idx]);
            m_player1->PlayerObject::pushButton(PlayerButton::Jump);
            m_player1->m_isDead = false;
            for (int i = 0; i < sinceClick + k + 1; i++) {
                GJBaseGameLayer::update(dt);
                if (m_player1->m_isDead) break;
            }
            if (!m_player1->m_isDead) { f->replaying = false; return; }
        }
        f->replaying = false;
        PlayLayer::destroyPlayer(p, obj); // no hubo forma: muere normal
    }
};

class $modify(CFPlayer, PlayerObject) {
    bool pushButton(PlayerButton b) {
        if (auto pl = static_cast<CFPlayLayer*>(PlayLayer::get())) {
            if (this == pl->m_player1 && !pl->m_fields->replaying)
                pl->m_fields->lastClick = pl->m_fields->frame;
        }
        return PlayerObject::pushButton(b);
    }
};
