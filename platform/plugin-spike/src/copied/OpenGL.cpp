// Generated for the HyprVX delivery spike (T066) from the patched draft tree (hyprland-draft 43b8c24e = Hyprland
// efb5099 + patches 0001-0009), src/render/OpenGL.cpp. Bodies are verbatim apart from: the class renamed to the spike subclass,
// HyprVX:: -> HyprVXSpike::, and the SRenderModifData::hyprvxCamera test -> HyprVXSpike::isCameraModif().
// Prologues from gen_copies.py PROLOGUES: none.
// Upstream code: Hyprland, BSD-3-Clause (docs/THIRD_PARTY.md). Do not edit by hand; regenerate with
// python3 -I platform/plugin-spike/gen_copies.py platform/plugin-spike/src/copied
#include "../SpikeAccess.hpp"

#include <GLES3/gl32.h>
#include <cstdint>
#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/path/Path.hpp>
#include <numbers>
#include <random>
#include <pango/pangocairo.h>
#include <src/render/OpenGL.hpp>
#include <src/render/Renderer.hpp>
#include <src/Compositor.hpp>
#include <src/helpers/MiscFunctions.hpp>
#include <src/helpers/CursorShapes.hpp>
#include <src/helpers/TransferFunction.hpp>
#include <src/config/ConfigValue.hpp>
#include <src/config/legacy/ConfigManager.hpp>
#include <src/pointer/PointerManager.hpp>
#include <src/desktop/view/LayerSurface.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/protocols/LayerShell.hpp>
#include <src/protocols/core/Compositor.hpp>
#include <src/protocols/ColorManagement.hpp>
#include <src/helpers/cm/ColorManagement.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/managers/eventLoop/EventLoopManager.hpp>
#include <src/pointer/cursor/CursorManager.hpp>
#include <src/helpers/fs/FsUtils.hpp>
#include <src/helpers/env/Env.hpp>
#include <src/helpers/MainLoopExecutor.hpp>
#include <src/i18n/Engine.hpp>
#include <src/event/EventBus.hpp>
#include <src/managers/screenshare/ScreenshareManager.hpp>
#include <src/notification/NotificationOverlay.hpp>
#include <src/state/MonitorState.hpp>
#include <src/errorOverlay/Overlay.hpp>
#include <src/helpers/Color.hpp>
#include <src/macros.hpp>
#include <src/render/pass/TexPassElement.hpp>
#include <src/render/pass/RectPassElement.hpp>
#include <src/render/pass/PreBlurElement.hpp>
#include <src/render/pass/ClearPassElement.hpp>
#include <src/render/GLRenderer.hpp>
#include <src/render/Shader.hpp>
#include <src/render/AsyncResourceGatherer.hpp>
#include <ranges>
#include <algorithm>
#include <fstream>
#include <string>
#include <xf86drm.h>
#include <fcntl.h>
#include <gbm.h>
#include <filesystem>
#include <cstring>
// not installed with the stock headers (build-time file): #include "./shaders/Shaders.hpp"
#include <src/render/ShaderLoader.hpp>
#include <src/render/Texture.hpp>
#include <src/render/gl/GLFramebuffer.hpp>
#include <src/render/gl/GLTexture.hpp>


#include "../Camera.hpp"
#include "../Hooks.hpp"

using namespace Hyprutils::OS;
using namespace NColorManagement;
using namespace Desktop::View;
using namespace Render;
using namespace Render::GL;

struct SpikeOpenGL : public Render::GL::CHyprOpenGLImpl {
        void renderBorder(const CBox& box, const Config::CGradientValueData& grad, SBorderRenderData data);
        void renderBorder(const CBox& box, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2, float lerp, SBorderRenderData data);

    static void hk_renderBorder0(Render::GL::CHyprOpenGLImpl* self, const CBox& box, const Config::CGradientValueData& grad, SBorderRenderData data) {
        static_cast<SpikeOpenGL*>(self)->renderBorder(box, grad, data);
    }
    static void hk_renderBorder1(Render::GL::CHyprOpenGLImpl* self, const CBox& box, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2, float lerp, SBorderRenderData data) {
        static_cast<SpikeOpenGL*>(self)->renderBorder(box, grad1, grad2, lerp, data);
    }
};

static ShaderFeatureFlags globalFeatures() {
    return g_pHyprRenderer->m_renderData.pMonitor && g_pHyprRenderer->m_renderData.pMonitor->needsUnmodifiedCopy() && g_pHyprRenderer->m_renderData.currentFB->getMirrorTexture() ?
        SH_FEAT_MIRROR :
        0;
}
static ShaderFeatureFlags getDecoFeatures() {
    const bool IS_ICC   = g_pHyprRenderer->workBufferImageDescription()->value().icc.present;
    const auto settings = g_pHyprRenderer->getCMSettings(g_pHyprRenderer->workBufferImageDescription(), getDefaultImageDescription(), nullptr, true,
                                                         g_pHyprRenderer->m_renderData.pMonitor->m_sdrMinLuminance, g_pHyprRenderer->m_renderData.pMonitor->m_sdrMaxLuminance);

    ShaderFeatureFlags features = SH_FEAT_ROUNDING | SH_FEAT_CM | globalFeatures();
    if (IS_ICC)
        features |= SH_FEAT_ICC;
    else {
        if (settings.needsTonemap) {
            features |= SH_FEAT_TONEMAP;
            if (settings.tonemapMode == 3)
                features |= SH_FEAT_ALT_TONEMAP;
        }
        if (settings.needsSDRmod)
            features |= SH_FEAT_SDR_MOD;
    }
    return features;
}
void SpikeOpenGL::renderBorder(const CBox& box, const Config::CGradientValueData& grad, SBorderRenderData data) {
    auto& m_renderData = g_pHyprRenderer->m_renderData;
    RASSERT((box.width > 0 && box.height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_renderData.pMonitor, "Tried to render rect without begin()!");

    TRACY_GPU_ZONE("RenderBorder");

    if (g_pHyprRenderer->m_renderData.damage.empty())
        return;

    CBox newBox = box;
    g_pHyprRenderer->m_renderData.renderModif.applyToBox(newBox);

    if (data.borderSize < 1)
        return;

    int scaledBorderSize = std::round(data.borderSize * m_renderData.pMonitor->m_scale);
    scaledBorderSize     = std::round(scaledBorderSize * g_pHyprRenderer->m_renderData.renderModif.combinedScale());

    // adjust box
    newBox.x -= scaledBorderSize;
    newBox.y -= scaledBorderSize;
    newBox.width += 2 * scaledBorderSize;
    newBox.height += 2 * scaledBorderSize;

    float       round = data.round + (data.round == 0 ? 0 : scaledBorderSize);

    const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(newBox);

    const auto  BLEND = m_blend;
    blend(true);

    WP<CShader> shader;

    const bool  skipCM = !m_cmSupported || !g_pHyprRenderer->workBufferImageDescription()->needsCM(getDefaultImageDescription());
    if (!skipCM) {
        shader = useShader(getShaderVariant(SH_FRAG_BORDER1, getDecoFeatures()));
        passCMUniforms(shader, getDefaultImageDescription());
    } else
        shader = useShader(getShaderVariant(SH_FRAG_BORDER1, SH_FEAT_ROUNDING | globalFeatures()));

    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniform4fv(SHADER_GRADIENT, grad.m_colorsOkLabA.size() / 4, grad.m_colorsOkLabA);
    shader->setUniformInt(SHADER_GRADIENT_LENGTH, grad.m_colorsOkLabA.size() / 4);
    shader->setUniformFloat(SHADER_ANGLE, sc<int>(grad.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0));
    shader->setUniformFloat(SHADER_ALPHA, data.a);
    shader->setUniformInt(SHADER_GRADIENT2_LENGTH, 0);

    CBox transformedBox = newBox;
    transformedBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    shader->setUniformFloat2(SHADER_TOP_LEFT, sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y));
    shader->setUniformFloat2(SHADER_FULL_SIZE, sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y));
    shader->setUniformFloat2(SHADER_FULL_SIZE_UNTRANSFORMED, sc<float>(newBox.width), sc<float>(newBox.height));
    shader->setUniformFloat(SHADER_RADIUS, round);
    shader->setUniformFloat(SHADER_RADIUS_OUTER, data.outerRound == -1 ? round : data.outerRound);
    shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);
    shader->setUniformFloat(SHADER_THICK, scaledBorderSize);

    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

    // calculate the border's region, which we need to render over. No need to run the shader on
    // things outside there
    // HyprVX: under the camera hint the drawn boxes have fractional edges; cover the whole border ring and skip only
    // the drawn (modified) window interior, not the logical one
    const bool HYPRVXCAMERA = HyprVXSpike::isCameraModif(m_renderData.renderModif);
    CBox       interior     = box;
    if UNLIKELY (HYPRVXCAMERA)
        m_renderData.renderModif.applyToBox(interior);
    interior.expand(-scaledBorderSize - round);
    CRegion borderRegion = g_pHyprRenderer->m_renderData.damage.copy().intersect(HYPRVXCAMERA ? HyprVXSpike::roundOutBox(newBox) : newBox);
    borderRegion.subtract(HYPRVXCAMERA ? HyprVXSpike::roundInBox(interior) : interior);

    if (g_pHyprRenderer->m_renderData.clipBox.width != 0 && g_pHyprRenderer->m_renderData.clipBox.height != 0)
        borderRegion.intersect(g_pHyprRenderer->m_renderData.clipBox);

    if (!borderRegion.empty()) {
        borderRegion.forEachRect([this](const auto& RECT) {
            scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    glBindVertexArray(0);

    blend(BLEND);
}
void SpikeOpenGL::renderBorder(const CBox& box, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2, float lerp, SBorderRenderData data) {
    auto& m_renderData = g_pHyprRenderer->m_renderData;
    RASSERT((box.width > 0 && box.height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_renderData.pMonitor, "Tried to render rect without begin()!");

    TRACY_GPU_ZONE("RenderBorder2");

    if (g_pHyprRenderer->m_renderData.damage.empty())
        return;

    CBox newBox = box;
    g_pHyprRenderer->m_renderData.renderModif.applyToBox(newBox);

    if (data.borderSize < 1)
        return;

    int scaledBorderSize = std::round(data.borderSize * m_renderData.pMonitor->m_scale);
    scaledBorderSize     = std::round(scaledBorderSize * g_pHyprRenderer->m_renderData.renderModif.combinedScale());

    // adjust box
    newBox.x -= scaledBorderSize;
    newBox.y -= scaledBorderSize;
    newBox.width += 2 * scaledBorderSize;
    newBox.height += 2 * scaledBorderSize;

    float       round = data.round + (data.round == 0 ? 0 : scaledBorderSize);

    const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(newBox);

    const auto  BLEND = m_blend;
    blend(true);

    WP<CShader> shader;
    const bool  skipCM = !m_cmSupported || !g_pHyprRenderer->workBufferImageDescription()->needsCM(getDefaultImageDescription());
    if (!skipCM) {
        shader = useShader(getShaderVariant(SH_FRAG_BORDER1, getDecoFeatures()));
        passCMUniforms(shader, getDefaultImageDescription());
    } else
        shader = useShader(getShaderVariant(SH_FRAG_BORDER1, SH_FEAT_ROUNDING | globalFeatures()));

    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniform4fv(SHADER_GRADIENT, grad1.m_colorsOkLabA.size() / 4, grad1.m_colorsOkLabA);
    shader->setUniformInt(SHADER_GRADIENT_LENGTH, grad1.m_colorsOkLabA.size() / 4);
    shader->setUniformFloat(SHADER_ANGLE, sc<int>(grad1.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0));
    if (!grad2.m_colorsOkLabA.empty())
        shader->setUniform4fv(SHADER_GRADIENT2, grad2.m_colorsOkLabA.size() / 4, grad2.m_colorsOkLabA);
    shader->setUniformInt(SHADER_GRADIENT2_LENGTH, grad2.m_colorsOkLabA.size() / 4);
    shader->setUniformFloat(SHADER_ANGLE2, sc<int>(grad2.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0));
    shader->setUniformFloat(SHADER_ALPHA, data.a);
    shader->setUniformFloat(SHADER_GRADIENT_LERP, lerp);

    CBox transformedBox = newBox;
    transformedBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    shader->setUniformFloat2(SHADER_TOP_LEFT, sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y));
    shader->setUniformFloat2(SHADER_FULL_SIZE, sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y));
    shader->setUniformFloat2(SHADER_FULL_SIZE_UNTRANSFORMED, sc<float>(newBox.width), sc<float>(newBox.height));
    shader->setUniformFloat(SHADER_RADIUS, round);
    shader->setUniformFloat(SHADER_RADIUS_OUTER, data.outerRound == -1 ? round : data.outerRound);
    shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);
    shader->setUniformFloat(SHADER_THICK, scaledBorderSize);

    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

    // calculate the border's region, which we need to render over. No need to run the shader on
    // things outside there
    // HyprVX: under the camera hint the drawn boxes have fractional edges; cover the whole border ring and skip only
    // the drawn (modified) window interior, not the logical one
    const bool HYPRVXCAMERA = HyprVXSpike::isCameraModif(m_renderData.renderModif);
    CBox       interior     = box;
    if UNLIKELY (HYPRVXCAMERA)
        m_renderData.renderModif.applyToBox(interior);
    interior.expand(-scaledBorderSize - round);
    CRegion borderRegion = g_pHyprRenderer->m_renderData.damage.copy().intersect(HYPRVXCAMERA ? HyprVXSpike::roundOutBox(newBox) : newBox);
    borderRegion.subtract(HYPRVXCAMERA ? HyprVXSpike::roundInBox(interior) : interior);

    if (g_pHyprRenderer->m_renderData.clipBox.width != 0 && g_pHyprRenderer->m_renderData.clipBox.height != 0)
        borderRegion.intersect(g_pHyprRenderer->m_renderData.clipBox);

    if (!borderRegion.empty()) {
        borderRegion.forEachRect([this](const auto& RECT) {
            scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    glBindVertexArray(0);
    blend(BLEND);
}

void HyprVXSpike::addCopiedOpenGLHooks(std::vector<SHookSpec>& specs) {
    specs.push_back({"renderBorder", "Render::GL::CHyprOpenGLImpl::renderBorder(Hyprutils::Math::CBox const&, Config::CGradientValueData const&, Render::GL::CHyprOpenGLImpl::SBorderRenderData)", reinterpret_cast<void*>(&SpikeOpenGL::hk_renderBorder0), nullptr});
    specs.push_back({"renderBorder", "Render::GL::CHyprOpenGLImpl::renderBorder(Hyprutils::Math::CBox const&, Config::CGradientValueData const&, Config::CGradientValueData const&, float, Render::GL::CHyprOpenGLImpl::SBorderRenderData)", reinterpret_cast<void*>(&SpikeOpenGL::hk_renderBorder1), nullptr});
}
