// Generated for the HyprVX plugin form S (Feature 002 T052; delivery spike T066) from the patched draft tree
// (hyprland-draft bf59c232 = Hyprland efb5099 + patches 0001-0013), src/render/ElementRenderer.cpp. Bodies are verbatim apart from: the class renamed to the spike subclass,
// HyprVX:: -> HyprVXSpike::, and the SRenderModifData::hyprvxCamera test -> HyprVXSpike::isCameraModif().
// Prologues from gen_copies.py PROLOGUES: drawSurface.
// Copy-only (not hooked; called by a wrapper as HyprVXSpike::copied_<name>): none.
// Upstream code: Hyprland, BSD-3-Clause (docs/THIRD_PARTY.md). Do not edit by hand; regenerate with
// python3 -I platform/plugin/gen_copies.py platform/plugin/src/mechanism/copied
#include "../SpikeAccess.hpp"

#include <src/render/ElementRenderer.hpp>
#include <src/render/Renderer.hpp>
#include <src/layout/LayoutManager.hpp>
#include <src/desktop/view/Window.hpp>
#include <src/render/pass/ClearPassElement.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>


#include "../Camera.hpp"
#include "../Hooks.hpp"

using namespace Render;

struct SpikeElementRenderer : public Render::IElementRenderer {
        void drawSurface(WP<CSurfacePassElement> element, const CRegion& damage);

    static void hk_drawSurface(Render::IElementRenderer* self, WP<CSurfacePassElement> element, const CRegion& damage) {
        static_cast<SpikeElementRenderer*>(self)->drawSurface(element, damage);
    }
};

void SpikeElementRenderer::drawSurface(WP<CSurfacePassElement> element, const CRegion& damage) {
    // HyprVX spike prologue (gen_copies.py PROLOGUES)
    if UNLIKELY (HyprVXSpike::isCameraModif(g_pHyprRenderer->m_renderData.renderModif) && !g_pHyprRenderer->m_renderData.clipBox.empty()) {
        g_pHyprRenderer->m_renderData.renderModif.applyToBox(g_pHyprRenderer->m_renderData.clipBox);
        g_pHyprRenderer->m_renderData.clipBox = HyprVXSpike::roundOutBox(g_pHyprRenderer->m_renderData.clipBox);
    }
    const auto&                   m_data       = element->m_data;
    auto&                         m_renderData = g_pHyprRenderer->m_renderData;

    Hyprutils::Utils::CScopeGuard x = {[]() {
        g_pHyprRenderer->m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        g_pHyprRenderer->m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
    }};

    if (!m_data.texture) {
        element->discard();
        return;
    }

    const auto& TEXTURE = m_data.texture;

    // this is bad, probably has been logged elsewhere. Means the texture failed
    // uploading to the GPU.
    if (!TEXTURE->ok()) {
        element->discard();
        return;
    }

    const auto INTERACTIVERESIZEINPROGRESS = m_data.pWindow && g_layoutManager->dragController()->target() && g_layoutManager->dragController()->mode() == MBIND_RESIZE;
    TRACY_GPU_ZONE("RenderSurface");

    auto        PSURFACE = Desktop::View::CWLSurface::fromResource(m_data.surface);

    const float ALPHA         = m_data.alpha * m_data.fadeAlpha * (PSURFACE ? PSURFACE->m_alphaModifier : 1.F);
    const float OVERALL_ALPHA = PSURFACE ? PSURFACE->m_overallOpacity : 1.F;
    const bool  BLUR          = m_data.blur && (!TEXTURE->m_opaque || ALPHA < 1.F || OVERALL_ALPHA < 1.F);

    auto        windowBox = element->getTexBox();

    const auto  PROJSIZEUNSCALED = windowBox.size();

    windowBox.scale(m_data.pMonitor->m_scale);
    windowBox.round();

    if (windowBox.width <= 1 || windowBox.height <= 1) {
        element->discard();
        return;
    }

    const bool MISALIGNEDFSV1 = std::floor(m_data.pMonitor->m_scale) != m_data.pMonitor->m_scale /* Fractional */ && m_data.surface->m_current.scale == 1 /* fs protocol */ &&
        windowBox.size() != m_data.surface->m_current.bufferSize /* misaligned */ && DELTALESSTHAN(windowBox.width, m_data.surface->m_current.bufferSize.x, 3) &&
        DELTALESSTHAN(windowBox.height, m_data.surface->m_current.bufferSize.y, 3) /* off by one-or-two */ &&
        (!m_data.pWindow || (!m_data.pWindow->sizeAnimation()->isBeingAnimated() && !INTERACTIVERESIZEINPROGRESS)) /* not window or not animated/resizing */ &&
        (!m_data.pLS || (!m_data.pLS->sizeAnimation()->isBeingAnimated())); /* not LS or not animated */

    calculateUVForSurface(m_data.pWindow, m_data.surface, m_data.pMonitor->m_self.lock(), m_data.mainSurface, windowBox.size(), PROJSIZEUNSCALED, MISALIGNEDFSV1);

    auto cancelRender = false;
    auto clipRegion   = element->visibleRegion(cancelRender);
    if (cancelRender) {
        element->discard();
        return;
    }

    // HyprVX (RC1): under the camera hint the quad is drawn at the modified box, so clip and damage must be too
    CBox damageBox = windowBox;
    if UNLIKELY (HyprVXSpike::isCameraModif(m_renderData.renderModif)) {
        m_renderData.renderModif.applyToBox(damageBox);
        damageBox = HyprVXSpike::roundOutBox(damageBox);
        m_renderData.renderModif.applyToRegion(clipRegion);
        // visibleRegion() pads by 1 px against rounding; the modifier scaled that pad by the zoom, so restore it
        clipRegion.expand(1);
    }

    // check for fractional scale surfaces misaligning the buffer size
    // in those cases it's better to just force nearest neighbor
    // as long as the window is not animated. During those it'd look weird.
    // UV will fixup it as well
    if (MISALIGNEDFSV1)
        m_renderData.useNearestNeighbor = true;

    float rounding      = m_data.rounding;
    float roundingPower = m_data.roundingPower;

    // HyprVX: corner radius follows the camera zoom
    if UNLIKELY (HyprVXSpike::isCameraModif(m_renderData.renderModif))
        rounding *= m_renderData.renderModif.combinedScale();

    rounding -= 1; // to fix a border issue

    if (m_data.dontRound) {
        rounding      = 0;
        roundingPower = 2.0f;
    }

    const bool WINDOWOPAQUE    = m_data.pWindow && m_data.pWindow->wlSurface()->resource() == m_data.surface ? m_data.pWindow->opaque() : false;
    const bool CANDISABLEBLEND = ALPHA >= 1.f && OVERALL_ALPHA >= 1.f && rounding <= 0 && WINDOWOPAQUE;

    if (CANDISABLEBLEND)
        g_pHyprRenderer->blend(false);
    else
        g_pHyprRenderer->blend(true);

    // FIXME: This is wrong and will bug the blur out as shit if the first surface
    // is a subsurface that does NOT cover the entire frame. In such cases, we probably should fall back
    // to what we do for misaligned surfaces (blur the entire thing and then render shit without blur)
    if (m_data.surfaceCounter == 0 && !m_data.popup) {
        if (BLUR)
            drawElement(makeShared<CTexPassElement>(CTexPassElement::SRenderData{
                            .tex                   = TEXTURE,
                            .box                   = windowBox,
                            .a                     = ALPHA,
                            .blurA                 = m_data.fadeAlpha,
                            .overallA              = OVERALL_ALPHA,
                            .round                 = rounding,
                            .roundingPower         = roundingPower,
                            .blur                  = true,
                            .blockBlurOptimization = m_data.blockBlurOptimization,
                            .allowCustomUV         = true,
                            .surface               = m_data.surface,
                            .wrapX                 = m_data.wrapX,
                            .wrapY                 = m_data.wrapY,
                            .discardMode           = m_data.discardMode,
                            .discardOpacity        = m_data.discardOpacity,
                            .clipRegion            = clipRegion,
                            .currentLS             = m_data.pLS,
                        }),
                        m_renderData.damage.copy().intersect(damageBox));
        else
            drawElement(makeShared<CTexPassElement>(CTexPassElement::SRenderData{
                            .tex            = TEXTURE,
                            .box            = windowBox,
                            .a              = ALPHA * OVERALL_ALPHA,
                            .round          = rounding,
                            .roundingPower  = roundingPower,
                            .discardActive  = false,
                            .allowCustomUV  = true,
                            .surface        = m_data.surface,
                            .wrapX          = m_data.wrapX,
                            .wrapY          = m_data.wrapY,
                            .discardMode    = m_data.discardMode,
                            .discardOpacity = m_data.discardOpacity,
                            .clipRegion     = clipRegion,
                            .currentLS      = m_data.pLS,
                        }),
                        m_renderData.damage.copy().intersect(damageBox));
    } else {
        if (BLUR && m_data.popup)
            drawElement(makeShared<CTexPassElement>(CTexPassElement::SRenderData{
                            .tex                   = TEXTURE,
                            .box                   = windowBox,
                            .a                     = ALPHA,
                            .blurA                 = m_data.fadeAlpha,
                            .overallA              = OVERALL_ALPHA,
                            .round                 = rounding,
                            .roundingPower         = roundingPower,
                            .blur                  = true,
                            .blockBlurOptimization = true,
                            .allowCustomUV         = true,
                            .surface               = m_data.surface,
                            .wrapX                 = m_data.wrapX,
                            .wrapY                 = m_data.wrapY,
                            .discardMode           = m_data.discardMode,
                            .discardOpacity        = m_data.discardOpacity,
                            .clipRegion            = clipRegion,
                            .currentLS             = m_data.pLS,
                        }),
                        m_renderData.damage.copy().intersect(damageBox));
        else
            drawElement(makeShared<CTexPassElement>(CTexPassElement::SRenderData{
                            .tex            = TEXTURE,
                            .box            = windowBox,
                            .a              = ALPHA * OVERALL_ALPHA,
                            .round          = rounding,
                            .roundingPower  = roundingPower,
                            .discardActive  = false,
                            .allowCustomUV  = true,
                            .surface        = m_data.surface,
                            .wrapX          = m_data.wrapX,
                            .wrapY          = m_data.wrapY,
                            .discardMode    = m_data.discardMode,
                            .discardOpacity = m_data.discardOpacity,
                            .clipRegion     = clipRegion,
                            .currentLS      = m_data.pLS,
                        }),
                        m_renderData.damage.copy().intersect(damageBox));
    }

    g_pHyprRenderer->blend(true);

    if (!g_pHyprRenderer->m_bBlockSurfaceFeedback)
        element->m_data.surface->presentFeedback(element->m_data.when, element->m_data.pMonitor->m_self.lock());
};

void IElementRenderer::preDrawSurface(WP<CSurfacePassElement> element, const CRegion& damage) {
    auto& m_renderData              = g_pHyprRenderer->m_renderData;
    m_renderData.clipBox            = element->m_data.clipBox;
    m_renderData.useNearestNeighbor = element->m_data.useNearestNeighbor;
    // HyprVX (RC1): the window clip box is in untransformed monitor pixels
    if UNLIKELY (HyprVXSpike::isCameraModif(m_renderData.renderModif) && !m_renderData.clipBox.empty()) {
        m_renderData.renderModif.applyToBox(m_renderData.clipBox);
        m_renderData.clipBox = HyprVXSpike::roundOutBox(m_renderData.clipBox);
    }
    g_pHyprRenderer->pushMonitorTransformEnabled(element->m_data.flipEndFrame);
    m_renderData.currentWindow = element->m_data.pWindow;

    drawSurface(element, damage);

    // add async (dmabuf) buffers to usedBuffers so we can handle release later
    // sync (shm) buffers will be released in commitState, so no need to track them here
    if (element->m_data.surface->m_current.buffer && !element->m_data.surface->m_current.buffer->isSynchronous())
        g_pHyprRenderer->m_usedAsyncBuffers.emplace_back(element->m_data.surface->m_current.buffer);

    m_renderData.clipBox            = {};
    m_renderData.useNearestNeighbor = false;
    g_pHyprRenderer->popMonitorTransformEnabled();
    m_renderData.currentWindow.reset();
}


void HyprVXSpike::addCopiedElementRendererHooks(std::vector<SHookSpec>& specs) {
    specs.push_back({"drawSurface", "Render::IElementRenderer::drawSurface(Hyprutils::Memory::CWeakPointer<CSurfacePassElement>, Hyprutils::Math::CRegion const&)", reinterpret_cast<void*>(&SpikeElementRenderer::hk_drawSurface), nullptr});
}
