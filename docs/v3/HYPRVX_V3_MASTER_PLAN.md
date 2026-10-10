# HyprVX V3 — Kế hoạch Kiến trúc, Phục hồi & Kỹ thuật (Architecture Approval Gate)

> **Trạng thái:** kiến trúc đã được user duyệt ngày 2026-10-10. Replan cộng tác Claude + Codex (P1–P10) đã duyệt
> và áp dụng ngày 2026-10-10 (`docs/v3/HYPRVX_V3_COLLABORATION_REPLAN_PROPOSAL.md`, mục O). Chuẩn bị Sprint 1 và sinh
> `tasks.md` đã được duyệt; **implementation Sprint 1 chỉ bắt đầu sau khi user duyệt `tasks.md`.**
> **Cập nhật 2026-10-11 (DV3-15):** user dừng mô hình cộng tác Claude + Codex. Claude Code là agent implementation duy
> nhất; mục O (trừ O5) và mọi bước handoff giữa hai agent hết hiệu lực. Các sửa kỹ thuật của replan (P1–P5, P10,
> infra-before-claims, K0, GATE-A, GATE-N, U0) vẫn giữ. `tasks.md` đã được user duyệt tại `966e6b6`; Sprint 1 đang chạy.
> **Ngôn ngữ:** giải thích bằng tiếng Việt, thuật ngữ kỹ thuật giữ tiếng Anh.
> **Worktree:** `/home/towfienes/Projects/HyprVX-v3`, branch `v3/spec-foundation` (tách từ V1 `dev@15f75b0`).
> **Tài liệu liên quan (Spec Kit):**
> - `.specify/memory/constitution.md` — 10 nguyên tắc bắt buộc.
> - `specs/001-native-spatial-camera/` — spec, plan, research (bằng chứng file:line), data-model, contracts, quickstart, checklist.
> - `research/` — bản sao read-only các patch V1/V2 kèm SHA-256 và provenance.
>
> **Mục lục:**
> - 0 Context
> - A Quyết định kiến trúc
> - B Audit V1/V2
> - C Blueprint
> - D UX
> - E Performance
> - F Test
> - G Safety
> - H Roadmap
> - I Sprint 1
> - J Decision log
> - K Quy trình Spec Kit
> - L Cấu trúc repo & build tái lập
> - M Tương thích & cập nhật
> - N Nhật ký thực hiện phiên lập kế hoạch
> - O Quy trình cộng tác Claude Code + Codex

---

## 0. Context — vì sao có kế hoạch này

Người dùng (CachyOS + Hyprland 0.56.2 + Caelestia 2.4.0) muốn thêm **Drift Mode** kiểu DriftWM vào Hyprland: canvas 2D vô hạn, pan/pinch-zoom bằng trackpad, tương tác thật với cửa sổ thật, rồi tắt về Hyprland bình thường mà không restart. Hai lần thử trước:

- **V1** (`/home/towfienes/Projects/HyprVX`, branch `dev`, HEAD `15f75b0`): core toán học + runtime tốt, nhưng chưa từng biến đổi pixel cửa sổ thật.
- **V2** (`/home/towfienes/Projects/HyprVX-v2`, `v2/drift-mvp` @ `31086d7`, nested repo `hyprland/` = upstream `efb5099` + 3 commit): báo "14/14 PASS" nhưng test bằng tay cho thấy surface không zoom, có artifact xám bậc thang.

Mục tiêu: dựng nền tảng V3 có thể bảo vệ bằng bằng chứng, chọn kiến trúc đúng, rồi dừng ở cổng duyệt kiến trúc.

### Môi trường host đã xác minh (read-only)
| Mục | Giá trị |
|---|---|
| Hyprland | 0.56.2-4.1, commit `efb50993780079460b0cbed1363e2166a2de1d9f` (khớp `/usr/include/hyprland/src/version.h`) |
| Libs | aquamarine 0.15.1, hyprutils 0.14.2, hyprgraphics 0.5.1, hyprcursor 0.1.13, hyprlang 0.6.8, wayland 1.26.0, wayland-protocols 1.49 |
| Toolchain | gcc 16.2.1, clang 23.1.1, cmake 4.4.4, meson 1.12.1, ccache, gtest 1.18, sdl3 3.4.18 |
| GPU | AMD HawkPoint iGPU `[1002:1900]` |
| RAM | 14 GiB, **~2.8 GiB khả dụng**, swap đang dùng 7.3 GiB → build Hyprland phải giới hạn `-j4` |
| Config | Hyprland dùng **Lua** (`~/.config/hypr/hyprland.lua`, `hl.*`); Caelestia tại `/etc/xdg/quickshell/caelestia` |
| Spec Kit | `specify` CLI 1.1.3 tại `~/.local/bin/specify` |
| Công cụ test | grim, slurp, foot, alacritty, kitty, nvtop có sẵn; **ydotool có sẵn nhưng CẤM dùng** (uinput tác động lên host) |

---

## A. Executive Architecture Decision

**HyprVX nên là:** một *view-transform layer* bên trong Hyprland. Drift Mode là **một view thuần túy** đặt lên mô hình cửa sổ có sẵn của Hyprland. Nó không phải WM thứ hai, không phải overlay vẽ hình chữ nhật giả, và không phải thumbnail.

**Chiến lược được chọn: Strategy C — Hybrid, "Patch the mechanism, plugin the policy".**

```
┌──────────────────────── hyprvx_core (V1, C++20, không deps) ────────────────────────┐
│ CameraMath (header-only, 1 phương trình duy nhất) · CameraController/inertia ·       │
│ World/Island (cluster) · SpatialNavigator · CameraHistory · ViewportFraming ·        │
│ Persistence (atomic JSON) · input semantic actions                                   │
└──────────────────────────────────────────────────────────────────────────────────────┘
            ▲ linked into (zero IPC on hot path)
┌──────── HyprVX controller (Hyprland plugin .so, nested trước) ────────┐
│ policy: mode state machine, gestures (cancellable gesture.* bus),     │
│ dispatchers, per-window world offsets, snap/cluster, persistence     │
└──────────────────────────────▲────────────────────────────────────────┘
                               │ IViewTransformProvider (seam API nhỏ)
┌──────── Narrow patch series trên Hyprland pinned v0.56.2 (efb5099) ───┐
│ mechanism: render hint PRE/POST_WINDOWS (compose, không replace),     │
│ drawSurface clip theo box đã transform, simplify/culling camera-aware,│
│ damage world→screen, hit-test (windowAt/windowSurfaceAt/              │
│ surfaceLocalAt), seat local coords, drag, popup reposition/onMap      │
└───────────────────────────────────────────────────────────────────────┘
```

**Vì sao chọn cách này:**
1. **Bằng chứng source (efb5099).** Zoom đúng đòi hỏi can thiệp khoảng 10 seam: render clip, `CRenderPass::simplify`, culling `shouldRenderWindow`/`visibleOnMonitor`, damage, `windowAt` (phải viết lại vì dòng 30 tra monitor theo `pos`), `surfaceLocalAt`, `CSeatManager::sendPointerMotion`, drag, `CPopup::reposition`/`onMap`. Upstream có bug clip (`ElementRenderer.cpp:327/345/367/385` intersect damage với `windowBox` *chưa transform*). Bug này phải sửa bằng patch hoặc hook, không có đường nào khác.
2. **Patch cho phép chứng minh nhanh và debug được.** Lỗi biên dịch hiện ra khi rebase. Không phụ thuộc trampoline x86_64 (`HookSystem.cpp:147,251`). `drawHints` bị inline nên không hook được.
3. **Plugin cho phép giữ policy tách khỏi mechanism.** Gestures (`gesture.swipe/pinch` cancellable, `EventBus.hpp:144-156`), dispatchers (`addDispatcherV2`), config (`addConfigValueV2`) đều là API chính thức. Policy vì vậy độc lập với cách phân phối mechanism.
4. **Quyết định phân phối production để sau (đã được user duyệt).** Cuối Phase 2 có một **plugin-hook port spike**: thử tái tạo seam bằng `createFunctionHook` trên các symbol đã export (đã kiểm bằng `objdump`: `windowAt`, `windowSurfaceAt`, `surfaceLocalAt`, `drawSurface`, `CRenderPass::simplify`, `damage*`, `CDragStateController::*`, `sendPointerMotion`...).
   - Nếu spike pass **cùng bộ test visual + input** → ship plugin chạy trên Hyprland gốc.
   - Nếu không → ship Hyprland vá chạy **song song** (prefix riêng, session riêng, Hyprland gốc vẫn là fallback).
5. **Đường dài hạn:** đề xuất upstream một API "view transform" tổng quát. Patch được viết như một feature generic (không mang tên HyprVX trong core render) để dễ upstream.

**Vì sao các phương án khác yếu hơn:**
- **A. Plugin thuần, chỉ dùng API chính thức:** không khả thi. `IWindowTransformer` render vào framebuffer cỡ monitor (`ElementRenderer.cpp:507,537`), loại trừ popup (`Transformer.hpp:15`, `Renderer.cpp:800`), không thể zoom-out, và không có inverse hit-test. `registerCallbackDynamic` là no-op (`PluginAPI.cpp:36-45`).
- **A'. Plugin bằng function hooks:** khả thi về kỹ thuật nhưng mong manh. Hook ~15 hàm nội bộ, phải viết lại `windowAt`; pointer constraints thực tế cần patch; mỗi bản Hyprland đổi chữ ký là gãy *âm thầm* lúc runtime thay vì lúc compile. Vẫn giữ làm ứng viên phân phối (spike ở Phase 2).
- **B. Fork đầy đủ:** chi phí bảo trì lớn nhất. Nó chính là con đường V2 đã đi: diff rải rác, công thức copy-paste 9 chỗ, ép `GIT_COMMIT_HASH`/`GIT_DIRTY=clean` (`build.sh:263-266`).
- **Pan bằng cách đổi logical position** (cách built-in scrolling layout làm, `ScrollingAlgorithm.cpp:554`): input tự đúng, nhưng không zoom được, phá layout/tiling, gây cửa sổ di cư giữa monitor/workspace, và phát sự kiện `movewindow` hàng loạt làm Caelestia nhiễu. Bị loại vì vi phạm tính khả nghịch.
- **Overlay vẽ lại (V1 Phase 3) / screenshot compositing:** không phải cửa sổ thật. Bị loại theo yêu cầu sản phẩm.
- **Chuyển sang DriftWM:** bị loại theo yêu cầu (giữ Hyprland + Caelestia). DriftWM còn là GPL-3.0, nên **không copy code**, chỉ tham chiếu hành vi.

**Cách kiến trúc giữ Hyprland + Caelestia:** Drift không đổi workspace ownership, không đổi logical geometry, không phát IPC event giả. Layer-shell (bar/OSD/notif/launcher/lock) được render ngoài window pass nên tự nhiên ở screen-space. Khi camera = identity và mode OFF, render path **không chèn hint nào**, tức là behaviorally stock.

### Ma trận quyết định (1 = kém, 5 = tốt)
| Tiêu chí | A plugin API | A' plugin hooks | B fork rộng | **C hybrid (chọn)** | Pan-by-logical-move |
|---|---|---|---|---|---|
| Khả thi kỹ thuật | 1 | 3 | 5 | **5** | 4 (không zoom) |
| Render/zoom đúng | 1 | 4 | 4 | **5** | 1 |
| Input đúng | 1 | 3 | 4 | **5** | 5 |
| Tương thích Caelestia | 5 | 5 | 4 | **5** | 2 |
| Runtime overhead | 5 | 4 | 4 | **5** | 3 |
| Memory ownership rõ | 3 | 3 | 3 | **5** | 3 |
| Bảo trì | 2 | 2 | 1 | **4** | 3 |
| Nâng version | 3 | 2 | 1 | **3** | 3 |
| Cô lập lỗi | 4 | 3 | 2 | **4** | 3 |
| Thời gian phát triển | — | 3 | 3 | **4** | 4 |
| Testability | 2 | 3 | 4 | **5** | 4 |
| Độ phức tạp deploy | 5 | 5 | 1 | **3** (quyết ở cổng Phase 2) | 5 |
| Khả nghịch | 5 | 4 | 3 | **5** | 2 |

### Giả định chưa xác minh → PoC cần chạy
| # | Giả định | Test ngắn nhất để kết luận |
|---|---|---|
| U1 | RC1+RC2 (xem B) là nguyên nhân artifact của V2 | Build V2 as-is trong nested, chụp `grim`, so với bản thêm `noSimplify` + clip đã transform |
| U2 | Camera identity + mode OFF ⇒ pixel-identical với stock | Diff ảnh stock-build vs patched-build, cùng scene, cùng client |
| U3 | Hiển thị cửa sổ của workspace ẩn mà không đổi ownership (Option B) | PoC: predicate `isDriftVisible` trong `shouldRenderWindow` + `windowAt`; quan sát frame callback, `wl_surface.enter`, focus → workspace switch |
| U4 | libinput còn gửi 3-finger swipe khi `drag_3fg=1` | Test phần cứng (Phase 4), log `gesture.swipe.begin` |
| U5 | Plugin hooks đủ để thay patch | Spike ở cuối Phase 2 |
| U6 | X11 override-redirect menu theo offset của parent | Test XWayland ở Phase 3 |

---

## B. Existing Project Audit (tóm tắt; bản đầy đủ có file:line: `specs/001-native-spatial-camera/research.md`)

### V1 (`HyprVX` @ `15f75b0`)
| Subsystem | Hiện trạng | Bằng chứng | Trạng thái | Dùng lại? | Cần đổi |
|---|---|---|---|---|---|
| Camera math | top-left `P`; `s=(w−P)·z`; `w=P+s/z`; anchor `P=w_a−a/z'` | `core/src/Camera.cpp:27-67` | Working (unit) | **Có** | Thêm monitor origin M + scale; tách thành header-only `CameraMath` |
| CameraController/inertia | damping exponential dạng đóng, frame-rate independent | `CameraMotion.cpp:236-426` | Working (unit) | **Có** | Flight zoom chưa anchor-stable; drag-pan chưa có release momentum (`RuntimeCameraController.cpp:71-89`) |
| World/Island/ownership | single-owner, join/detach có rollback | `World.cpp`, `Island.cpp:14-23` | Working (unit) | **Có** | Bịt lỗ `World::island()` trả `Island*` mutable |
| IslandInteractionController | state machine dwell/hysteresis | `IslandInteraction.cpp` | Chỉ test dùng (simulator copy lại) | Có (làm nền cluster) | Ánh xạ sang semantics "implicit snap cluster" của DriftWM |
| SpatialNavigator / History / Framing | cone ±65°, history 64, fit | `SpatialNavigation.cpp:190-345`, `NavigationState.cpp`, `ViewportFraming.cpp` | Working (unit) | **Có** | — |
| MultiOutput/Topology | camera mỗi output; tên eDP-2/DP-1 hard-code | `OutputTopology.cpp:49-163` | Sim-only | Một phần | Bỏ hard-code; feed monitor event thật |
| socket2/parser/mirror/bridge | observer read-only, epoch | `daemon/src/*` | Working (unit + replay) | Có (công cụ quan sát/test) | Không nằm trên hot path V3 |
| SpatialProjector | geometry **tự chế** 1000×700 | `SpatialProjection.cpp:162-177` | Không phải geometry thật | Chỉ ý tưởng "workspace region" | Dùng làm layout ban đầu cho unified world |
| Persistence | atomic write, fsync, .bak, .corrupt.N | `ProjectionStateStore.cpp:318-356` | Working (unit) | **Có** | Lưu world offset + camera |
| Input adapters | semantic actions, coalescing | `RuntimeNavigationInput.cpp` | Working (unit) | **Có** | Gắn vào `gesture.*` bus trong plugin |
| SDL simulators | 4 app, helper vẽ bị nhân bản | `simulator/*` | Sim-only, không test | Công cụ dev | Không mở rộng |
| NativeSession overlay | layer-shell vẽ hình chữ nhật | `platform/src/NativeSession.cpp` | Experimental, không test | Không cho Drift | Giữ như debug HUD tùy chọn |
| HyprlandPlugin | 7 listener, refuse `enable` | `HyprlandPlugin.cpp:116-121` | Experimental | Event bridge + check commit | Cần check ABI *và* patchset ID |
| NestedGuard + harness | marker 0600 + SO_PEERCRED PID | `NestedGuard.cpp:35-102`, `tools/hyprvx_nested.py` | Pure fn tested; I/O chưa test | **Có (nền harness V3)** | Khôi phục stale session; chạy binary vá; test I/O |
| Phase 4 patch (untracked) | camera trên CMonitor, hit-test/drag/damage | `platform/patches/hyprland-v0.56.2-spatial.patch` (437 dòng) | **Inert**: không gì set camera, chưa từng build | Toán học có | Không có control API; warp sai; replace modifier; chọn monitor không nhất quán; thiếu RC1/RC2 |
| Build | pkg-config Hyprland + SDL3 bắt buộc | `CMakeLists.txt:161-211` | **Stale**: `build/CMakeCache.txt:554-574` trỏ `/tmp/hyprland-src` | — | Build dir mới; SDL/Hyprland optional; `pkg_get_variable(wayland-protocols pkgdatadir)` |
| Tests | 284 GTest + 13 Python + 1 CMake = 298 | grep | Toàn math/state/parse/fake socket | — | **Không test nào đo compositor thật** |
| `.kilo/worktrees/foremost-columnist` | bản copy cũ @ fb2f0a5 | — | Trùng lặp | Không | Để nguyên (không xóa trong phiên này) |

### V2 (`HyprVX-v2` @ `31086d7`, `hyprland/` @ `14d1232`)
| Subsystem | Hiện trạng | Bằng chứng | Trạng thái | Dùng lại? | Cần đổi |
|---|---|---|---|---|---|
| Camera state + 13 dispatchers | `CMonitor::m_spatialCamera` | `Monitor.hpp:40-44,381`; `KeybindManager.cpp:123-266` | Working (state) | Ý tưởng | Một helper chung thay cho 9 bản copy |
| Phương trình | render/inverse/damage/drag **nhất quán** | `Renderer.cpp:1185-1187` | Đúng toán | Có | — |
| Render hint | push trước window pass, reset sau pinned | `Renderer.cpp:1181-1246` | Đến được surface draw | Một phần | Compose với geometry modifier; đưa dim rect ra khỏi transform |
| **RC1 surface clip** | damage ∩ `windowBox` chưa transform | `ElementRenderer.cpp:327,345,367,385`; `OpenGL.cpp:1574-1594` | **Broken (confirmed source)** | — | Clip theo box đã transform |
| **RC2 simplify/culling** | boundingBox/opaqueRegion chưa transform | `Pass.cpp:45-87`; `SurfacePassElement.cpp:117-137` | **Broken (confirmed source)** | — | `noSimplify` (MVP) → camera-aware bbox (Phase 7) |
| Giả thuyết "renderModif bị ghi đè" | `drawHints` gán nhưng không có hint nào khác giữa push/reset | `ElementRenderer.cpp:190-194` | **Bác bỏ cho default path** | — | Chỉ đúng khi `renderWorkspace` có geometry ≠ identity |
| Hit-test window/popup/subsurface | inverse đúng | `ViewHitTester.cpp:35,269,296` | Working (source) | Có | Chọn monitor thống nhất |
| Decoration/drag/touch/tablet/constraint/warp | lẫn world/screen | `Window.cpp:458`; `DragController.cpp:106-198`; `Touch.cpp:196` | Broken | Một phần | Inverse transform |
| Damage | surface/window đúng; `damageBox/Region` (border/shadow) sai; `IS_LAYER` sót popup layer | `Renderer.cpp:2761-2799` | Partial | Có | Hoàn thiện |
| Harness | SO_PEERCRED **chỉ lúc start**; `stop` kill PID không re-check | `run-nested.sh:274-300, 137-191` | Mỏng | Ý tưởng | Re-verify mỗi lệnh; `HYPRLAND_NO_SD_VARS=1` |
| "14/14 PASS" | không có pixel/pointer check; #3, #10 luôn pass; #9 đọc JSON trước khi bật DRIFT | `scripts/e2e-spatial-test.sh:89-159` | **Unverified, vô giá trị như bằng chứng visual** | Không | Thay bằng test pixel + virtual-pointer |
| Provenance | ép `GIT_COMMIT_HASH=efb5099`, `GIT_DIRTY=clean` | `build.sh:263-266` | Sai provenance | Không | Đánh dấu patchset ID thật |
| Reproducibility | patch = diff nested (đã xác minh IDENTICAL) | format-patch diff | Working | Có | Script fetch efb5099 + `git am` |
| Teamwork gate | GATE_STATUS PENDING, milestone 3–5 chưa tick | `.agents/teamwork/*` | Claim chưa từng qua gate | — | — |

**Kết luận phục hồi:**
- **Nền tảng mạnh nhất** = V1 core + V1 harness/guard + phương trình nhất quán của V2 + bản đồ seam từ upstream efb5099.
- Viết **lại** patch series V3 sạch trên efb5099, dùng V1 patch / V2 diff làm tham chiếu (cả hai đều là code của user nên không vướng license). Không merge mù.

---

## C. Architecture Blueprint

### C1. Mô hình tọa độ — một phương trình duy nhất
Ký hiệu cho mỗi monitor m: `M` = logical origin, `s` = scale, `T` = output transform.

- Với mỗi cửa sổ `w`: `L(w)` = logical box do Hyprland sở hữu (**không bao giờ bị Drift sửa**), `O(w)` = world offset do HyprVX sở hữu.
- **World:** `W(w) = L(w) + O(w)`.
- **Camera theo (monitor, scene):** `C = (P, z)`, trong đó `P` là điểm world nằm ở góc trên-trái monitor và `z ∈ [z_min, z_max]`.

```
Forward  (render, damage, culling):  S = M + (W − P)·z          [screen logical]
Physical:                            px = T((S − M)·s)
Inverse  (hit-test):                 W = P + (S − M)/z
Surface-local (wl_pointer):          local = W − W(surface_origin)   (đơn vị surface, KHÔNG nhân z)
Anchor zoom tại điểm a:              W_a = P + (a−M)/z ;  P' = W_a − (a−M)/z'
Pan theo delta màn hình d:           P' = P − d/z
Identity:                            O ≡ 0, P = M, z = 1  ⇒  S = L  (stock)
```

**Invariants** (GTest + test native):
1. `inverse(forward(W)) = W` với sai số ≤ 1e-9.
2. Anchor cố định: `forward(W_a)` trước và sau zoom lệch ≤ 1e-9.
3. Identity ⇒ không có hint element nào được thêm (kiểm bằng counter debug).
4. Hit-test pixel `S` trả về surface mà render vẽ tại `S` (±1 px).
5. Damage của mọi thứ window-owned = `forward(box)` làm tròn ra ngoài +1 px.
6. Layer-shell **không bao giờ** qua forward.
7. `z` hữu hạn, nằm trong biên; NaN/Inf ⇒ panic-reset về identity.

Cài đặt: `hyprvx/CameraMath.hpp` header-only, không deps. Dùng chung cho `hyprvx_core` (thay `Camera.cpp`) và patch Hyprland (vendored, license permissive). Không subsystem nào được viết lại phương trình riêng.

### C2. Normal vs Drift
```
NORMAL: Hyprland stock path. Plugin listeners tồn tại nhưng return ngay (không cancel).
        Không hint, không noSimplify, không damage thêm.  Overhead ≈ 1 branch / frame.

DRIFT:  scene = {workspaces được include, O(w), C(m)}
        render: [bg layers][bottom layers] ─ HINT(camera, compose) ─ [windows+popups+subsurf]
                ─ RESET ─ [top/overlay layers][layer popups][lock][cursor]
        input:  S → (layer test, screen space) → inverse → windowAt_drift → local
```

### C3. Rendering pipeline (seam trong `renderAllClientsForWorkspace`, efb5099 `Renderer.cpp:1102-1252`)
- Hint ở `RENDER_PRE_WINDOWS` / reset trước `RENDER_POST_WINDOWS` (stage order đã xác minh, §5 của research). Hint **compose** với geometry modifier đang có.
- `drawSurface`: clip = damage ∩ `applyToBox(windowBox)`; `visibleRegion` cũng được transform.
- MVP: `noSimplify = true` khi camera ≠ identity. Phase 7: `boundingBox`/`opaqueRegion` camera-aware.
- Culling: `shouldRenderWindow`/`visibleOnMonitor` dùng visible-world-rect `[P, P + size/z]`.
- Fullscreen window trong Drift → render với camera identity (hành vi đã ghi tài liệu).
- Direct scanout/solitary: chặn khi camera ≠ identity (`m_directScanoutBlocked`).
- Dim rect của special workspace được đưa ra ngoài hint. Bán kính bo góc × z. Blur: Phase 7 xác minh.

**Per-window vs workspace pass:** chọn **một hint cho cả window pass + offset per-window O(w)** (gộp vào `renderdata.pos`, tương tự cách `m_renderOffset` đang được cộng ở `Renderer.cpp:562`). Không dùng offscreen per-window: offscreen tốn VRAM, cắt theo cỡ monitor, và loại popup.

### C4. Input pipeline
```
pointer S ─► layers (screen space, giữ nguyên) ─► nếu Drift: windowAt(S) viết lại:
            monitor = monitorAt(S) (theo S, KHÔNG theo world) ─► W = inverse(S)
            ─► duyệt cửa sổ của scene (theo z-order) test W ∈ W(w) / popup / subsurface
            ─► local = W − W(surface) ─► setPointerFocus / sendPointerMotion(local)
drag (Drift move): HyprVX sở hữu → đổi O(w) (không đổi L) ; delta_world = delta_screen / z
resize: floating → resize thật (configure) ; tiled → MVP: không hỗ trợ trong Drift (ghi rõ)
constraints/warp: warp_screen = forward(warp_world) ; confine region qua forward
touch/tablet: cùng inverse (Phase 3)
```

### C5. Window/world ownership — thiết kế cho phép Option B (yêu cầu cuối)
- **Hyprland sở hữu:** workspace membership, `L(w)`, tiled/floating, layout tree, focus. Drift **không ghi** vào những thứ này. Ngoại lệ có chủ đích: focus do user click, và resize floating do user yêu cầu.
- **HyprVX sở hữu:** `O(w)`, camera, cluster/snap, pin, bookmark. Lưu theo identity (logic khôi phục identity của V1 persistence).
- **MVP (Option A):** scene = {workspace đang active trên monitor}, `O(w)=0` lúc bật, camera riêng cho mỗi workspace.
- **Unified (Option B, yêu cầu sản phẩm cuối):**
  - scene = mọi workspace thường (không special). `O(w)` ban đầu = workspace region offset (ý tưởng `SpatialProjector` của V1: các workspace xếp cạnh nhau trong world).
  - Predicate `isDriftVisible(w, m)` thay cho kiểm tra "workspace visible" trong render và hit-test.
  - Click cửa sổ thuộc workspace k ⇒ Hyprland focus ⇒ workspace k thành active. Caelestia hiển thị đúng sự thật, camera không nhảy, animation workspace bị suppress trong Drift.
  - **Không đổi ownership.**
  - Tắt Drift ⇒ bỏ `O`, scene trở về workspace active ⇒ layout, ownership, tiling y nguyên vì chưa từng bị sửa.
- **Rủi ro B cần PoC (U3):** frame callback và `wl_surface.enter` cho cửa sổ workspace ẩn; side effect của `focusWindow` → `changeworkspace` (warp cursor, animation); client tự throttle; số lượng cửa sổ render tăng (culling bắt buộc).
- **Phương án dự phòng nếu U3 thất bại:** "drift workspace" vật lý, tức move tất cả cửa sổ vào 1 workspace và restore khi tắt. Cách này phá dwindle tree, gây event storm, Caelestia nhiễu. Chỉ dùng khi PoC chứng minh không còn đường khác, và phải được user duyệt riêng.

### C6. Caelestia integration (audit `/etc/xdg/quickshell/caelestia`, read-only)
| Thành phần | Loại | Hành vi trong Drift |
|---|---|---|
| Bar, drawers, OSD, notifications, launcher, background | `WlrLayershell` (10 chỗ; Background/Bottom/Top/Overlay) | Screen-space tự nhiên (render ngoài hint) |
| Lock | `WlSessionLock` (3) | Render sau trong `renderMonitor`, screen-space |
| Popout focus | `HyprlandFocusGrab` (2) | Không đổi |
| FileDialog, nexus WindowFactory | `FloatingWindow` (2) = **xdg_toplevel** | Là cửa sổ world như app. Policy: spawn ở tâm viewport |
| `services/Hypr.qml` | `activeToplevel`, `focusedWorkspace`, `toplevels`, refresh theo `rawEvent` | Drift không phát event giả, không đổi ownership ⇒ dữ liệu luôn đúng |

Kết luận: **không cần compat layer cho MVP.** Compat layer tùy chọn (Phase 6) chỉ để hiển thị trạng thái Drift trên bar, và nằm trong thư mục riêng của HyprVX. **Không sửa file Caelestia production.**

### C7. Mode transition (state machine)
```
OFF ──enable──► ENTERING (validate: seam ABI/patchset ID, camera hợp lệ, monitor OK)
                  │ fail → OFF + notification
                  ▼
                DRIFT ──disable / panic / invalid state / monitor removed──► EXITING
                  ▲                                                         │
                  └────────────── (camera về identity, scene bỏ, O giữ trong state) ◄─┘──► OFF
```
Mọi transition idempotent. `hyprvx:panic` luôn đưa về OFF trong 1 frame.

---

## D. UX & Interaction Specification (điểm khởi đầu, chốt sau hardware test)

**Xung đột đã phát hiện trong config user** (read-only):
- `~/.config/hypr/config/inputs.lua:14-17`: 4 ngón ngang = workspace; 3 ngón xuống = **close**; 3 ngón lên = fullscreen; 3 ngón trái = float.
- `gestures.lua`: 3 ngón lên = special.
- `hypr-user.lua:7`: `drag_3fg = 1`.

Vì vậy khi ở Drift, **HyprVX phải cancel mọi gesture 3 ngón** (qua bus `gesture.*`) để pan không bao giờ kích hoạt "close window".

| Hành động | NORMAL | DRIFT (đề xuất, theo mô hình context-aware của DriftWM) |
|---|---|---|
| Bật/tắt | `Super+<key>` (chọn sau) / dispatcher `hyprvx:toggle` | như trái |
| Pan | — | 2-finger scroll trên **canvas trống**; `Super`+2-finger scroll ở bất kỳ đâu; 3-finger swipe (nếu U4 pass); `Super`+LMB drag |
| Zoom | — | 2-finger pinch trên canvas; `Super`+pinch ở bất kỳ đâu; 3-finger pinch; `Super`+wheel. Luôn neo tại cursor |
| Pinch trên cửa sổ | forward cho app | forward cho app (ví dụ browser zoom) trừ khi giữ `Super` |
| Context | — | xác định **lúc gesture begin**, khóa đến end; cancel ⇒ không commit |
| Momentum | — | release velocity × friction dạng đóng (V1 `CameraMotion`); tham số "drift" 0–1 |
| 4-finger ngang | workspace | MVP: giữ workspace (Option A). Unified: đánh giá lại |
| Move window | Hyprland bình thường | `Super`+LMB drag ⇒ đổi `O(w)`; giữ `Shift` ⇒ cả cluster |
| Snap | — | ngưỡng tính bằng **screen px** (24), gap/break theo world (12 / 32); có hysteresis |
| Pin | — | `pinned_to_screen`: bỏ qua camera, nằm trên các cửa sổ khác; unpin ⇒ đặt `O` để giữ vị trí đang nhìn thấy |
| Navigation | — | `Super+arrow` hướng; Home; center focused; zoom-to-fit (cửa sổ / cluster); bookmarks 1–4; history back/forward (V1) |
| New window | — | spawn ở tâm viewport: `O(w) = center_world − center(L(w))` |
| Fullscreen | bình thường | render identity khi fullscreen |
| Escape | — | `Super+Escape` ⇒ OFF; `hyprvx:panic`; NaN/biên ⇒ auto reset; mất monitor ⇒ OFF |
| Zoom range | — | MVP `[0.1, 2.0]`. Ở z<1 texture bị minify (có thể aliasing), ở z>1 upscale bị mờ (chưa re-render theo scale) — **hành vi trình bày đã ghi tài liệu** |

---

## E. Performance Engineering Plan
- **CPU:** không daemon mới trên hot path. Controller nằm trong plugin, zero IPC mỗi frame, không file I/O mỗi frame. Gesture delta được coalesce vào frame schedule. Physics chỉ chạy khi có input, velocity hoặc transition (V1 đã có `needsFrame`).
- **RAM/VRAM:**
  - Metadata ước tính < 1 KiB/cửa sổ, nên plugin PSS mục tiêu ≤ 10 MiB.
  - Không offscreen buffer per-window, nên VRAM thêm ≈ 0. Đo bằng `/proc/<pid>/smaps_rollup` và `nvtop`/`amdgpu` fdinfo.
- **Damage:** camera thay đổi ⇒ full damage (không tránh được khi mọi thứ đều di chuyển). Camera đứng yên ⇒ damage partial **đã transform** ⇒ cursor blink trong app không gây full redraw.
- **Culling:** visible-world-rect. Chưa cần spatial index; chỉ thêm khi benchmark ≥ 50 cửa sổ cho thấy cần.
- **Budgets đề xuất** (chưa đo; phải đo baseline stock trước):

| Kịch bản | Budget |
|---|---|
| Normal idle | CPU tick thêm = 0 ± noise trong 60 s; frame thêm = 0 |
| Drift idle (camera tĩnh) | 0 frame thêm khi client không damage |
| Drift pan/zoom, 10 cửa sổ | 120 Hz: p99 frame time ≤ 8.33 ms. 210 Hz: p95 ≤ 4.76 ms (mục tiêu, không hứa cho mọi workload) |
| Toggle × 1000 | RSS/PSS tăng ≤ 1 MiB; không leak |

- **Phương pháp đo:**
  - Client đo đặt trong nested/hardware dùng `wp_presentation` feedback để lấy p50/p95/p99 khoảng frame.
  - CPU qua `/proc/<pid>/stat`.
  - Dùng Tracy build của Hyprland (submodule có sẵn) cho per-pass timing.
  - Input latency chỉ đo trên phần cứng: log timestamp evdev → presentation trong debug build.
  - `perf`/`heaptrack` **chưa cài**; cài cần user duyệt (pacman).
  - Mỗi số đi kèm điều kiện: AC/pin, monitor, refresh, số cửa sổ, nhiệt độ/thời gian chạy; lặp ≥ 5 lần, báo median + khoảng.
- **Regression:** idle CPU, p99 frame time vượt baseline > 10% hoặc memory tăng ⇒ chặn feature work (theo `docs/PERFORMANCE.md` V1).

### E1. Phương pháp benchmark chi tiết

**Điều kiện phần cứng:** mỗi lần đo ghi vào `benchmarks/runs/<date>-<id>/conditions.json` các trường sau:
- Output: tên, mode, refresh, scale, HDR on/off.
- Nguồn điện: AC hay pin (`/sys/class/power_supply/*/online`).
- CPU governor/EPP, chỉ **đọc**.
- Nhiệt độ ban đầu.
- Số cửa sổ và loại client.
- Build: binary sha256, patchset ID.

**Không bao giờ** thay governor, độ sáng hay cấu hình GPU để đo.

**Workload chuẩn** (định nghĩa bằng script, tái lập được):

| ID | Mô tả |
|---|---|
| W0 | Idle, 0 client (chỉ nền) |
| W2 / W10 / W20 | N `hyprvx-testclient` tĩnh, kích thước 800×600, xếp lưới cố định |
| W10-anim | 10 client, trong đó 2 client tự vẽ lại 60 fps (mô phỏng video/cursor blink) |
| P-pan | pan liên tục 2000 px/s trong 10 s (driver: dispatcher dev hoặc virtual pointer + `Super`-drag) |
| P-zoom | zoom 1.0 → 0.1 → 2.0 → 1.0 trong 10 s, anchor tại tâm |
| T-toggle | 1000 lần ON/OFF, mỗi lần cách 50 ms |
| A-scroll | app scroll (testclient phản hồi `wl_pointer.axis`) trong lúc camera đứng yên và trong lúc pan |

**Metrics và công cụ:**

| Metric | Cách đo | Ghi chú |
|---|---|---|
| CPU compositor | `utime+stime` từ `/proc/<pid>/stat`, lấy mẫu đầu/cuối cửa sổ 60 s | Không polling nội bộ; chỉ đọc 2 lần |
| Frame interval p50/p95/p99, dropped | client `hyprvx-presentation-probe` dùng `wp_presentation` feedback (`presented` vs `discarded`) | Chạy trong nested trước, sau đó trên phần cứng |
| Thời gian render mỗi pass | Hyprland build với Tracy (`-DUSE_TRACY`), chỉ ở bản profiling | Không bật ở bản đo latency |
| Memory | `/proc/<pid>/smaps_rollup` (Pss, Private_Dirty) trước/sau T-toggle | Plugin overhead = chênh lệch so với stock cùng workload |
| VRAM | `/proc/<pid>/fdinfo/*` (`drm-memory-*` trên amdgpu) + `nvtop` | — |
| Input latency | chỉ trên phần cứng: timestamp libinput event (log debug) → `presented` timestamp | Không đo được trung thực trong nested |
| Năng lượng | `/sys/class/power_supply/BAT*/power_now` trung bình 5 phút, W0 và P-pan | Pin, độ sáng giữ nguyên như người dùng đang dùng |

**Baseline:** luôn đo **stock-from-source** (cùng commit, cùng cờ build) trước, sau đó patched OFF, rồi patched DRIFT. Báo cáo dạng median và [min, max] của ≥ 5 lần chạy. Sai số đo được nêu rõ, ví dụ độ phân giải tick là 10 ms.

**Ngưỡng regression:**
- Patched OFF so với stock: CPU idle chênh lệch nằm trong khoảng noise đo được giữa 2 lần chạy stock; p99 frame interval không tệ hơn quá 2%.
- Drift: theo budget ở mục E.


---

## F. Test & Verification Plan (chống lặp lại "14/14 PASS")
**Quy tắc:**
- Mọi kết quả phải là PASS / FAIL / BLOCKED / NOT RUN / NOT AUTOMATABLE, kèm artifact: log, ảnh, JSON, commit, binary sha256.
- Dispatcher trả `ok` **không bao giờ** là bằng chứng.
- Không test nào được "luôn pass".

| Level | Nội dung | Công cụ |
|---|---|---|
| L1 Static | clean build từ checkout sạch, patchset ID, `git diff --check`, clang-tidy | script build |
| L2 Unit | `CameraMath` invariants 1–7 (property test ngẫu nhiên), V1 284 GTest | GTest |
| L3 Sim | V1 simulator (giữ nguyên) | — |
| L4 Native | nested boot, client map, transform thấy được, click/focus/scroll, popup, layer isolation, OFF | harness V3 |
| L5 Visual | `grim` chụp **nested** output → analyzer đo bbox cửa sổ, tỉ lệ scale, displacement, marker 4 góc (không clip), so với render full-damage tham chiếu (phát hiện stale pixel) | `hyprvx-testclient` + analyzer |
| L5b Input | `hyprvx-vpointer` (zwlr_virtual_pointer_v1 **trên display nested**) bắn điểm S → testclient log surface-local → so với `inverse(S) − W(surface)` ±1 px | testclient JSON log |
| L6 Physical | trackpad thật, checklist có người quan sát + video | NOT AUTOMATABLE, ký xác nhận |
| L7 Reliability | 1000 toggle, open/close khi zoom, popup, crash client, crash nested, memory growth | soak script |
| L8 Production candidate | chỉ khi user duyệt riêng | — |

**Test nền tảng bắt buộc:**
- **T-ID:** camera identity ⇒ ảnh patched build **pixel-identical** với stock build cùng scene.
- **T-Z025:** z = 0.25 ⇒ bbox cửa sổ = 0.25 × kích thước ± 2 px, đúng vị trí theo phương trình, đủ 4 marker.

**Kiểm soát an toàn trong test:** CẤM `ydotool`/uinput. Mọi tool input/capture phải kết nối display nested đã được verify PID.

---

## G. Safety & Rollback Plan
- **Harness V3** (kế thừa V1 `NestedGuard` + `hyprvx_nested.py`, vá các lỗ của V2):
  - Runtime dir riêng `/tmp/hvx-*`.
  - Marker 0600; **re-verify SO_PEERCRED PID + start time + signature ≠ host trước MỌI lệnh**, kể cả `stop`.
  - `HYPRLAND_NO_SD_VARS=1`; D-Bus nested riêng (`dbus-run-session`).
  - Binary vá chạy từ prefix project-local, **không bao giờ** từ `$PATH`/`/usr/bin`.
  - Log riêng được giữ lại (không xóa khi stop).
  - Có lệnh `recover` cho stale session (compositor đã chết).
  - Giới hạn tài nguyên: `systemd-run --user --scope -p MemoryMax=…` cho nested + build.
- **Runtime rollback:** `hyprvx:panic`, Super+Escape, auto-reset khi state không hợp lệ, OFF khi monitor bị gỡ, plugin unload ⇒ stock.
- **Production (sau này, cần duyệt):**
  - Hyprland gốc giữ nguyên trong `/usr/bin`, session gốc vẫn là default.
  - HyprVX là session/plugin opt-in; kiểm tra compatibility lúc startup (hash + patchset ID + ABI string). Fail thì không kích hoạt.
  - Chống crash-loop: đếm số lần start trong `$XDG_STATE_HOME/hyprvx`, quá ngưỡng ⇒ không load.
  - Khôi phục qua TTY được ghi tài liệu từng bước.
  - Btrfs snapshot (đã có từ V1) trước mọi bước host.
- **Phiên này:** chỉ ghi tài liệu trong worktree V3. Không đụng V1 working tree (ngoài việc `git worktree add` ghi metadata vào `.git` chung), không đụng V2, không đụng host.

---

## H. Phased Roadmap (critical path in đậm)
Effort là khoảng thô theo đơn vị *focused engineering-weeks*, độ tin cậy thấp; sẽ hiệu chỉnh sau Phase 1.

| Phase | Mục tiêu | Phụ thuộc | Rủi ro chính | Effort | Cổng hoàn thành | Chưa làm |
|---|---|---|---|---|---|---|
| **0 Evidence & Recovery** | worktree V3, Spec Kit, research docs, script pin efb5099 + apply patch, build stock-from-source + V2-as-is trong nested | — | RAM thấp khi build | 0.5–1 | Clean build tái lập được + U1 xác nhận bằng ảnh | Mọi feature |
| **1 Architecture Proof** | `CameraMath` + patch 0001–0004 (render seam, clip, simplify, culling, damage) + testclient/vpointer/analyzer | 0 | Seam ẩn ngoài danh sách 10 | 1–2 | **T-ID pixel-identical + T-Z025 pass** | gesture, snap |
| **2 Native Spatial Camera** | input seam (hit-test, local, chọn deco/góc theo inverse, warp, constraints; move/resize-drag bị chặn khi camera ≠ identity), 1 cửa sổ thật đúng visual + input; **spike plugin-hook ⇒ quyết định phân phối** | 1 | Constraint/XWayland | 1–3 | L4+L5+L5b pass cho 1 cửa sổ; báo cáo spike | Multi-window; move bằng `O(w)` |
| U0 Unified-world feasibility spike | Trả lời U3 bằng bằng chứng: predicate `isDriftVisible` trong `shouldRenderWindow` + `windowAt` trên branch bỏ đi; quan sát frame callback, `wl_surface.enter`, focus → workspace switch, warp/animation side effect | 2 (cổng Feature 001) | Side effect focus/workspace | 0.5–1 (time-boxed) | Báo cáo `docs/v3/U0_FEASIBILITY_REPORT.md` với PASS/FAIL/BLOCKED từng câu hỏi; **không merge code** | Không implement U; không đổi ownership |
| **3 Multi-Window MVP (Option A)** | plugin controller + `O(w)`, popups, subsurfaces, XWayland, focus, move (Drift) bằng drag sửa `O(w)`, OFF sạch, camera per workspace | 2 (U0 chạy trước hoặc đầu phase này) | Popup off-monitor (`CPopup::onMap` null) | 2–4 | Kịch bản 12 bước của user (trừ trackpad) chạy được trong nested | Unified world |
| **4 Trackpad UX** | gesture arbitration, anchor zoom, momentum, cancel; **hardware validation** | 3 | `drag_3fg`, bind sẵn có | 1–3 | Checklist L6 do người ký | Snap |
| 5 DriftWM window behavior | snap, implicit cluster (từ V1 Island), pin, nav, bookmarks, zoom-to-fit | 4 | Oscillation snap | 2–4 | Test L2 + L5 cho snap/cluster | — |
| 6 Caelestia compat | chứng minh screen-space, workspace/focus/launcher; compat layer tùy chọn | 3 | FloatingWindow placement | 0.5–1 | Checklist Caelestia | Sửa file Caelestia |
| 7 Performance & Stability | baseline, camera-aware simplify, damage tối ưu, soak, leak | 3 | p99 ở 210 Hz trên iGPU | 2–3 | Budget mục E đạt hoặc ghi rõ lý do | — |
| **U Unified World (Option B)** | PoC U3 → scene đa workspace không đổi ownership | 3, 7 | Side effect focus/workspace | 2–5 | Acceptance U-1..U-6 bên dưới | Không được hạ cấp thành overview |
| 8 Multi-monitor/HDR | camera độc lập mỗi output, hotplug, scale/HDR | 7 | HDR/CM | 2–4 | Ma trận topology | — |
| 9 Production candidate | packaging, compat check, crash-loop guard, rollback, duyệt user | tất cả | — | 1–2 | L8 có user duyệt | — |

**Shortest critical path đến MVP dùng được:** 0 → 1 → 2 → 3 → 4, sau đó 6 + 7 (smoke). Unified world (U) là **yêu cầu sản phẩm cuối, không phải tùy chọn**. Spike U0 (sau cổng Feature 001) phát hiện sớm rủi ro U3 trước khi đầu tư Phase 4–5; nó không thay thế Milestone U. Nếu U0 FAIL, phương án "drift workspace vật lý" vẫn cần user duyệt riêng (C5).

**Acceptance cho Unified World (U):**
- **U-1:** Cửa sổ từ ≥ 3 workspace hiển thị cùng lúc, đúng vị trí world.
- **U-2:** Click cửa sổ thuộc workspace k ⇒ focus đúng; Caelestia báo k active; camera không nhảy.
- **U-3:** Trong Drift không có event `movewindow`/`movewindowv2`, ngoài thao tác do user.
- **U-4:** Tắt Drift ⇒ `hyprctl clients -j` (nested) trước và sau giống hệt về `workspace.id`, `at`, `size`, `floating`, `fullscreen`.
- **U-5:** Pan/zoom giữa các vùng workspace không có stale pixel (L5).
- **U-6:** Overview tách biệt **không** được tính là hoàn thành U.

---

## I. First Implementation Sprint (`tasks.md` đã duyệt tại `966e6b6`; đang thực thi, Claude-only theo DV3-15)
**Mục tiêu:** chốt rủi ro cao nhất, tức chứng minh có thể render surface thật bị scale/pan đúng pixel mà Normal vẫn pixel-identical.

**Điều chỉnh theo replan (P7, 2026-10-10):**
- **Infra-before-claims** thay cho "test infra trước code": patch được viết song song với tool; không patch nào được
  đánh giá hay claim trước khi tool sẵn sàng và baseline stock đã tất định.
- ~~**Hai lane** (mục O)~~ — hết hiệu lực từ DV3-15 (2026-10-11): Claude Code làm toàn bộ (contract, patch, tooling,
  test, debug, tích hợp). R1 thành kiểm chứng clean-room do Claude chạy lại từ build mới (T044).
- **Checkpoint mới:** K0 (contract freeze: `transform-contract.md` v1.1, `test-tooling.md` v1.0), GATE-A (stock vs
  stock ×3 pixel-identical, V-23), GATE-N (V-24; chỉ PASS khi đủ 5 điều kiện S1 setup hợp lệ, S2 camera thực sự áp
  dụng, P analyzer PASS trên ảnh tốt, N analyzer FAIL đúng loại lỗi trên V2-as-is, H xác nhận bằng mắt; bảng phân
  loại analyzer hỏng / setup sai / V2 không tái hiện ở `contracts/test-tooling.md` §6.1).
- `tools/vpointer` thuộc Phase 2 (T054), **không** làm trong Sprint 1 (sửa 2026-10-11 để tôn trọng cổng duyệt Phase 2).
- Evidence: raw ở `~/.cache/hyprvx/<agent>/evidence/<run-id>/`, Claude import + `sha256sum -c` vào archive
  `~/.local/share/hyprvx/evidence/<agent>/<run-id>/` trước khi tick task (`contracts/test-tooling.md` §8).
- License: `CameraMath.hpp` mang `SPDX-License-Identifier: BSD-3-Clause` (DV3-10), nên patch 0001 không còn bị chặn.
- Tái lập được báo cáo ở 3 mức tách biệt: input / test-result / binary (`contracts/test-tooling.md` §3).

1. **Chuẩn bị nguồn (an toàn):** `scripts/fetch-hyprland.sh`
   - Clone `hyprwm/Hyprland` vào `~/.cache/hyprvx/src/hyprland-efb5099`, kèm submodules (hyprland-protocols, udis86).
   - Verify `git rev-parse HEAD == efb50993…`.
   - `scripts/apply-patches.sh` dùng `git am patches/hyprland/*.patch` lên branch `hyprvx/0.56.2`.
2. **Build:**
   - `scripts/build-hyprland.sh`: CMake + Ninja, `-j4`, ccache, `-DNO_HYPRPM=TRUE`, prefix `~/.cache/hyprvx/prefix/<patchset-id>`.
   - `GIT_COMMIT_HASH` giữ thật; `GIT_DIRTY="hyprvx-<sha256(patches)[:12]>"`.
   - Build 2 biến thể: **stock-from-source** và **patched**. Ghi `build-info.json` (sha256 binary, versions lib).
   - Chạy trong `systemd-run --user --scope -p MemoryMax=6G`.
3. **Test infra trước claim** (viết song song với patch; schema trong `specs/001-native-spatial-camera/contracts/test-tooling.md`):
   - `tools/testclient` (C++, xdg_toplevel SHM): vẽ nền đặc, 4 marker màu ở góc, lưới 50 px; log JSON `enter/motion/button` surface-local.
   - `tools/vpointer`: zwlr_virtual_pointer_v1, absolute motion. **Thuộc Phase 2 (T054), không làm trong Sprint 1.**
   - `tools/analyze_frame.py`: chạy `python -I`, đọc PNG (Pillow nếu có, nếu không thì PPM qua `grim -t ppm`), tìm bbox theo màu marker.
   - Harness: port `hyprvx_nested.py` + re-verify mỗi lệnh + `--binary` trỏ prefix.
4. **Baseline bằng chứng:** build V2 patch as-is ⇒ chụp z = 0.5 / 0.25 ⇒ kỳ vọng thấy clip/stale (xác nhận U1). Lưu ảnh vào `evidence/`.
5. **Patch series V3** (sạch trên efb5099):
   - `0001` `CameraMath.hpp` + state camera per monitor (chưa đổi render).
   - `0002` render seam: hint PRE/POST_WINDOWS compose; `drawSurface` clip transformed; `noSimplify` khi ≠ identity; dim rect ngoài hint; chặn direct scanout.
   - `0003` culling `visibleOnMonitor`/`shouldRenderWindow` theo visible-world-rect.
   - `0004` damage `damageSurface/Window/Box` cho window-owned.
   - `0005` dispatcher debug `hyprvx-dev:camera <Px> <Py> <z>`, chỉ build khi `HYPRVX_DEV`.
6. **Tests:**
   - **T-ID:** stock vs patched identity, diff = 0.
   - **T-Z025 / T-Z05 / T-Z2:** bbox ± 2 px, marker đủ.
   - **T-PAN:** displacement đúng.
   - **T-STALE:** 50 trạng thái camera ngẫu nhiên; mỗi trạng thái so ảnh partial-damage với ảnh sau `damageMonitor` full, diff ≤ ngưỡng.
   - **T-STALE-STATIC:** camera cố định ≠ identity, client cập nhật vùng nhỏ theo thứ tự xác định; mỗi frame partial-damage so với frame full-damage cùng trạng thái client (spec US2.5).
   - **T-Z2** dùng cửa sổ 400×300 để box đã transform nằm gọn trong output 1280×800 (spec US2.2).
   - **T-OFF:** về identity ⇒ ảnh bằng ảnh T-ID.
   - Input chưa nằm trong sprint 1; chỉ ghi chú observation.
7. **Kết quả kỳ vọng:** cửa sổ foot/testclient co đúng 25%, không cắt, không stale; layer-shell (testclient layer tùy chọn) không đổi.
8. **Success criteria:** GATE-A và GATE-N PASS; T-ID, T-Z*, T-PAN, T-STALE, T-STALE-STATIC, T-OFF đều PASS kèm artifact; L2 invariants PASS (gồm tương đương V1); clean build tái lập 2 lần cho cùng sha256 đầu vào patch (mức input + test-result); R1 (T044) là rebuild clean-room do Claude chạy lại, kèm báo cáo kiểm từ source, cho cùng kết quả (DV3-15).
9. **Stop conditions:**
   - T-ID không pixel-identical.
   - Build đòi đổi package hệ thống.
   - Harness không verify được PID.
   - Seam cần vượt ngoài danh sách 10 điểm ⇒ dừng, báo cáo, quyết định lại.
   - OOM/swap thrash ⇒ dừng.

---

## J. Decision Log
**Đã chốt (user duyệt trong phiên này):**
- **DV3-01** Strategy C hybrid; phân phối production quyết định tại cổng spike Phase 2.
- **DV3-02** V3 nằm trong worktree riêng `/home/towfienes/Projects/HyprVX-v3`, branch `v3/spec-foundation` từ `dev@15f75b0`. V1 working tree giữ nguyên. V2 là tham chiếu read-only.
- **DV3-03** MVP = Option A; **Option B (unified world) là yêu cầu sản phẩm cuối**, thiết kế không gắn chết vào 1 workspace.

**Đề xuất trong plan này (duyệt cùng plan):**
- **DV3-04** Một phương trình camera (C1) trong `CameraMath.hpp` dùng chung.
- **DV3-05** Drift = view thuần túy: `W = L + O`, Hyprland model bất biến.
- **DV3-06** Patch series viết lại sạch; V2 diff và V1 Phase 4 patch chỉ làm tham chiếu.
- **DV3-07** Không copy code DriftWM (GPL-3.0).
- **DV3-08** Spec Kit làm quy trình (mục K).

**Đã chốt trong replan cộng tác (user duyệt 2026-10-10):**
- **DV3-09** Chiến lược Hybrid Claude Code + Codex theo phase; Claude là technical lead và integration owner (mục O).
- **DV3-10** `CameraMath.hpp` mang `SPDX-License-Identifier: BSD-3-Clause` (chỉ file này).
- **DV3-11** Feature 001 chặn move/resize-drag khi camera ≠ identity; move bằng `O(w)` thuộc Feature 002 (FR-012).
- **DV3-12** Spike U0 time-boxed, chỉ bằng chứng, sau cổng Feature 001.
- **DV3-13** Ủy quyền commit thường trực trên branch `v3/001-*` (không push, không cleanup phá hủy, không merge vào
  `dev`/`main`/`v3/spec-foundation` khi chưa duyệt).
- **DV3-14** Codex Auto-review cho escalation trong phạm vi §O3 (workspace-write + on-request giữ nguyên; nested
  compositor/GPU/socket host vẫn duyệt tay; không Full Access/Bypass), áp dụng từ WO-04.
- **DV3-15** Dừng cộng tác Claude + Codex (user, 2026-10-11): Claude Code là agent implementation duy nhất; mục O hết
  hiệu lực trừ O5; DV3-09 (hybrid) và DV3-14 bị thay thế; công việc và evidence đã có được giữ nguyên.

**Còn mở:**
- License toàn dự án (D010 của V1). `CameraMath.hpp` đã chốt BSD-3-Clause (DV3-10).
- Phím bật/tắt Drift.
- Có tắt `drag_3fg` khi ở Drift hay không.
- Zoom range cuối cùng.
- Resize cửa sổ tiled trong Drift.

**Blockers:** không có cho phiên tài liệu. Sprint 1 cần ~6 GiB RAM rảnh cho build.

**Cần user duyệt sau này:** cài `perf`/`heaptrack` (pacman); mọi bước chạm host; deploy production; phương án "drift workspace vật lý" nếu U3 thất bại.

---

## K. Quy trình Spec Kit (đã thiết lập)

Spec Kit CLI 1.1.3 được khởi tạo trong worktree V3 bằng lệnh `specify init --here --integration claude --script sh`. Preview chạy trước trong scratchpad để chắc chắn lệnh không ghi đè `AGENTS.md`, `.ai/` hay file nào khác: nó chỉ tạo `.claude/skills/speckit-*` và `.specify/`. Script `create-new-feature.sh` của bản 1.1.3 **không** tạo git branch; feature hiện hành được trỏ bởi `.specify/feature.json` (file gitignored).

**Vòng đời mỗi feature (skill trong Claude Code):**
```
/speckit-specify   → specs/NNN-name/spec.md          (WHAT/WHY, acceptance, không chọn công nghệ)
/speckit-clarify   → giải quyết [NEEDS CLARIFICATION] (khi cần)
/speckit-plan      → plan.md, research.md, data-model.md, contracts/, quickstart.md (Constitution Check)
/speckit-checklist → checklists/*.md                  (chất lượng yêu cầu)
/speckit-tasks     → tasks.md                         (CHỈ sau khi user duyệt sprint)
/speckit-analyze   → kiểm tra nhất quán spec/plan/tasks với constitution (vi phạm = CRITICAL)
/speckit-implement → thực thi tasks                   (CHỈ sau khi user duyệt)
```

**Ánh xạ roadmap sang feature Spec Kit:**

| Feature dir | Phase | Trạng thái |
|---|---|---|
| `specs/001-native-spatial-camera/` | 0 + 1 + 2 (Sprint 1 nằm trong này) | spec + plan + research + contracts (replan P1–P5 đã áp); `tasks.md` trên branch `v3/001-native-spatial-camera`, đã duyệt (`966e6b6`), Sprint 1 đang thực thi |
| `specs/002-multi-window-drift-mvp/` | 3 (gồm move bằng drag sửa `O(w)`, DV3-11) | tạo khi 001 qua cổng |
| `specs/003-trackpad-native-ux/` | 4 | — |
| `specs/004-drift-window-behavior/` | 5 | — |
| `specs/005-caelestia-compatibility/` | 6 | — |
| `specs/006-performance-stability/` | 7 | — |
| `specs/007-unified-spatial-world/` | U (Option B, yêu cầu cuối) | — |
| `specs/008-multi-monitor-hdr/` | 8 | — |
| `specs/009-production-candidate/` | 9 | — |

**Cách dùng từ fish** (Spec Kit script là Bash, chạy qua `bash`):
```fish
cd /home/towfienes/Projects/HyprVX-v3
set -x SPECIFY_FEATURE_DIRECTORY specs/001-native-spatial-camera   # tùy chọn, ghi đè feature.json
bash .specify/scripts/bash/check-prerequisites.sh --json
```

---

## L. Cấu trúc repo, build tái lập & branching

**Cấu trúc đích của V3** (tiến hóa từ V1, không viết lại core):
```
HyprVX-v3/
  core/                 hyprvx_core (V1)  + core/include/hyprvx/core/CameraMath.hpp (mới, header-only, permissive)
  daemon/               runtime observer V1 (công cụ quan sát/replay, không nằm trên hot path)
  simulator/            V1 SDL simulator (đóng băng, chỉ sửa khi core đổi API)
  platform/             nested guard, native session (V1), plugin controller V3 (mới: platform/plugin/)
  patches/hyprland/     0001-…patch … (series V3, git am trên efb5099)   ← nguồn sự thật của mechanism
  scripts/              fetch-hyprland.sh, apply-patches.sh, build-hyprland.sh, nested.sh (Bash, shebang)
  tools/                testclient/, vpointer/, presentation-probe/, analyze_frame.py, hyprvx_nested.py
  tests/                unit (V1 + CameraMath), native/ (L4–L5b, chạy trên nested), soak/
  benchmarks/           workloads/, runs/ (gitignored raw data), reports/
  research/             v1-phase4/, v2-drift-mvp/, v1-build-stale/ (read-only, có SHA256SUMS)
  specs/                Spec Kit features
  docs/v3/              master plan (file này)
  .specify/ .claude/    Spec Kit
```

**Pipeline build tái lập:**
```
fetch-hyprland.sh  : git clone --recurse-submodules (hyprland-protocols, udis86) → checkout efb5099
                     → verify `git rev-parse HEAD` == efb50993… ; src ở ~/.cache/hyprvx/src (ngoài repo)
apply-patches.sh   : git switch -c hyprvx/0.56.2 efb5099 ; git am patches/hyprland/*.patch
                     patchset-id = sha256(cat patches/hyprland/*.patch)[:12]
build-hyprland.sh  : systemd-run --user --scope -p MemoryMax=6G -- cmake -G Ninja -B build-<id>
                     -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNO_HYPRPM=TRUE
                     -DCMAKE_INSTALL_PREFIX=~/.cache/hyprvx/prefix/<id> ; ninja -j4 ; install
                     GIT_COMMIT_HASH thật ; GIT_DIRTY="hyprvx-<id>"
                     → build-info.json {upstream, patchset_id, binary_sha256, lib versions (pacman -Q), compiler}
hyprwayland-scanner: không có trong pacman → build từ tag pin (v0.4.6, đã dùng ở V2) vào ~/.cache/hyprvx/tools
                     (không cài hệ thống)
```
- **Không vendor** cây Hyprland 2.6 GB vào git. Nguồn sự thật = commit pin + patch series.
- Hai lần build sạch (rm build dir) phải cho ra cùng `patchset-id`. Binary sha256 có thể khác do timestamp; chấp nhận nếu `build-info.json` khớp và test L4/L5 cho cùng kết quả.
- CMake của V1: thêm option `HYPRVX_WITH_SDL`, `HYPRVX_WITH_WAYLAND_PLATFORM`, `HYPRVX_WITH_HYPRLAND_PLUGIN` (mặc định OFF cho core-only); mỗi option gate cả target lẫn dependency discovery tương ứng (`CMakeLists.txt:161-211` hiện REQUIRED vô điều kiện). Dùng `pkg_get_variable(... wayland-protocols pkgdatadir)` thay cho đường dẫn tuyệt đối `/usr/share/...`. Configure headless (cả ba OFF) chỉ cần toolchain, GTest, nlohmann_json, Python 3. Luôn dùng build dir mới (`build-v3/`).
- Mỗi agent có cache riêng `~/.cache/hyprvx/<agent>/{src,build,prefix,ccache,tools}`; không dùng chung source tree, prefix hay ccache (`contracts/test-tooling.md` §1).
- CI: khả thi cho L1–L3 (container Arch, không GPU). L4+ cần GPU và Wayland host, nên chỉ chạy local, có script tái lập.

**Branching:**
- `dev` (V1) đóng băng như snapshot tham chiếu; không commit V3 vào đó.
- `v3/spec-foundation`: tài liệu, constitution, specs.
- Mỗi feature Spec Kit được phát triển trên `v3/NNN-name`, tách từ `v3/spec-foundation` (tạo branch thủ công khi được duyệt), rồi merge `--no-ff` sau khi qua cổng.
- Không rebase hay force-push branch chung. Không xóa tag `phase-*` của V1.
- V2 không bao giờ được merge; chỉ đọc tham chiếu trong `research/`.

---

## M. Tương thích, cập nhật Hyprland & giới hạn MVP

**Chính sách tương thích:**
1. HyprVX chỉ kích hoạt khi **cả ba** khớp: `GIT_COMMIT_HASH` (upstream), patchset ID (nếu chạy bản vá) và chuỗi ABI `__hyprland_api_get_hash()` (`<hash>_aq_0.15_hu_0.14_hg_0.5_hc_0.1_hlg_0.6`). Không khớp ⇒ plugin ở trạng thái inactive và gửi notification; Normal mode không bị ảnh hưởng.
2. CachyOS là rolling release, nên **mỗi lần pacman cập nhật Hyprland hoặc lib ABI** (aquamarine/hyprutils/…) thì quy trình sau phải chạy:
   - fetch commit mới
   - `git am` thử, sửa conflict
   - build
   - L1–L5 trong nested
   - chỉ khi PASS mới cài vào prefix mới; prefix cũ được giữ làm last-known-good.
3. Nếu chọn phân phối plugin-hook (cổng Phase 2): hyprpm-style rebuild theo hash. Hook bị lệch chữ ký ⇒ plugin tự vô hiệu, vì có kiểm tra `findFunctionsByName` kèm demangled signature trước khi hook.
4. Đường giảm chi phí dài hạn: đề xuất upstream "view transform" API. Mỗi seam được upstream là một patch bớt đi.

**Launch-blocking (phải xong trước khi được coi là MVP):**
- T-ID pixel-identical.
- Không clip, không stale.
- Hit-test và surface-local ±1 px.
- Popup định vị đúng.
- OFF khôi phục 100%.
- Panic hoạt động.
- Layer-shell ở screen-space.
- Không crash khi popup map ngoài monitor.
- Gesture 3 ngón không bao giờ kích hoạt bind "close".

**Giới hạn MVP được chấp nhận (ghi rõ cho người dùng):**
- Một output active.
- Resize cửa sổ tiled trong Drift chưa hỗ trợ.
- Zoom ≠ 1 thì chất lượng texture bị minify/upscale.
- Blur trong Drift có thể lệch (Phase 7).
- XWayland override-redirect chưa xác minh (U6).
- Screen sharing/recording ghi lại output đã transform (đúng ngữ nghĩa); toplevel capture không bị ảnh hưởng.
- Fullscreen thoát camera.
- HDR/CM chưa xác minh (Phase 8).
- Steam/game với pointer constraint: khuyến nghị Drift OFF; chưa xác minh.

---

## N. Nhật ký thực hiện phiên lập kế hoạch (2026-10-10)

| Bước | Kết quả | Bằng chứng |
|---|---|---|
| Audit V1/V2/upstream (read-only) | Hoàn tất | mục B; `specs/001-native-spatial-camera/research.md` |
| Kiểm tra trước worktree | `HyprVX-v3` chưa tồn tại; branch `v3/spec-foundation` chưa tồn tại | output `git show-ref` |
| Inventory file untracked V1 | `hyprland-v0.56.2-spatial.patch` sha256 `542ea179…9603`; `NATIVE_PHASE4.md` 0 byte | `research/v1-phase4/SHA256SUMS` |
| `git worktree add -b v3/spec-foundation …HyprVX-v3 15f75b0` | Thành công; `git status` của V1 không đổi (vẫn 2 file untracked) | `git worktree list` |
| Copy patch tham chiếu V1, V2 | sha256 bản sao = bản gốc | `research/*/SHA256SUMS`, `PROVENANCE.md` |
| Spec Kit init (preview rồi `--here`) | Chỉ tạo `.claude/`, `.specify/`; không file tracked nào bị sửa | `git status` |
| Constitution v1.0.0 | Đã viết | `.specify/memory/constitution.md` |
| Feature 001 | spec/plan/research/data-model/contracts/quickstart/checklist | `specs/001-native-spatial-camera/` |
| Hyprland/Caelestia host | **Không đụng tới.** Không chạy hyprctl, Hyprland, pacman, ydotool | — |
| Build/test V1 | **NOT RUN.** Phiên này chỉ làm tài liệu; build sạch thuộc Phase 0 | — |

---

## O. Quy trình cộng tác Claude Code + Codex (replan P8, duyệt 2026-10-10)

> **HẾT HIỆU LỰC từ 2026-10-11 (DV3-15).** User dừng mô hình cộng tác; Claude Code là agent duy nhất. O1–O4 và O6 chỉ
> còn là lịch sử. Vẫn giữ như quy tắc kỹ thuật: O5 (tài nguyên máy: heavy lock, ngưỡng RAM, PSI, `MemoryMax=6G`, `-j4`),
> layout cache `~/.cache/hyprvx/<agent>/` với `HYPRVX_AGENT=claude`, và quy tắc evidence/archive của
> `contracts/test-tooling.md` §8. Clone `HyprVX-v3-codex` giữ nguyên làm tham chiếu lịch sử, không đồng bộ nữa.

Chi tiết và lý do nằm trong `docs/v3/HYPRVX_V3_COLLABORATION_REPLAN_PROPOSAL.md` (mục A–J). Spec Kit vẫn là workflow
duy nhất; không có hệ thống task thứ hai.

### O1. Vai trò
- **Claude Code:** technical lead, integration owner. Sở hữu contract, `CameraMath.hpp`, `Camera.cpp`, patch series,
  debug compositor, `specs/` (gồm `tasks.md`, `verification.md`, `integration-log.md`), `docs/`, `.ai/`, `.specify/`.
- **Codex:** lane độc lập có đầu ra kiểm chứng được. Trong Sprint 1: `CMakeLists.txt`, `.gitignore`, `scripts/`,
  `tools/`, `tests/native/`, `tests/unit/test_camera_math.cpp`; review độc lập ở cổng (`codex review`, chạy lại suite).
- Mỗi file chỉ có một owner tại một thời điểm. Chuyển quyền phải ghi vào `integration-log.md`. Claude không sửa test
  của Codex; Codex không sửa patch của Claude. Owner của lane được quyết lại ở mỗi `/speckit-tasks`.

### O2. Ghi task trong Spec Kit
- Owner ghi ở cuối description: `- [ ] T014 [P] … in tools/testclient/ (owner: codex)`.
- Chỉ Claude sinh, sửa và tick `tasks.md`. Claude chạy `/speckit-implement` với filter owner của mình. Codex nhận
  work order (task ID, contract, lệnh verify, ranh giới file) và không chạy skill Spec Kit.

### O3. Codex chạy trong clone riêng, không truy cập `.git` dùng chung (least privilege)
Kiểm chứng ngày 2026-10-10 bằng `codex sandbox -P :workspace` trên repo thử trong scratchpad: sandbox
`workspace-write` của Codex đặt `.git` của workspace ở chế độ **read-only** (cả worktree lẫn clone). `git commit`,
ghi `.git/hooks` và ghi `.git/config` đều trả `Read-only file system`. Vì vậy cách cũ trong đề xuất (mục H1:
`git worktree` + `--add-dir …/HyprVX/.git`) **bị thay thế**. Nó tắt một cơ chế an toàn có chủ đích và cho Codex
quyền ghi `hooks/` và `config` (thực thi code trong lệnh git sau này của user hoặc Claude), cùng mọi ref của repo
dùng chung với V1 (`dev`, `main`, `v3/spec-foundation`).

Thiết lập được duyệt (Claude tạo clone khi Sprint 1 implementation được duyệt; repo chỉ 2.3 MB):
```fish
git clone --no-hardlinks --branch v3/001-native-spatial-camera \
    /home/towfienes/Projects/HyprVX /home/towfienes/Projects/HyprVX-v3-codex
git -C /home/towfienes/Projects/HyprVX-v3-codex switch -c v3/001-codex
git -C /home/towfienes/Projects/HyprVX-v3-codex remote set-url --push origin DISABLED
mkdir -p ~/.cache/hyprvx/codex ~/.cache/hyprvx/claude ~/.cache/hyprvx/locks \
         ~/.local/share/hyprvx/evidence/claude ~/.local/share/hyprvx/evidence/codex
touch ~/.cache/hyprvx/locks/heavy.lock

# User tự khởi động Codex (không đổi config, auth hay model mặc định):
codex -C /home/towfienes/Projects/HyprVX-v3-codex -s workspace-write -a on-request \
      --add-dir ~/.cache/hyprvx/codex -m <model>
```
- `--no-hardlinks`: object của clone không chung inode với repo V1/V3, nên ghi qua clone không thể sửa object dùng chung.
- Không `--add-dir` cho bất kỳ `.git` nào. `git commit` và `git fetch` trong clone sẽ cần user duyệt từng lệnh
  (escalation `on-request`); user thấy đúng lệnh trước khi chạy. Các lệnh đó chỉ chạm `.git` của chính clone.
- `--add-dir` chỉ cấp `~/.cache/hyprvx/codex` (source/build/prefix/ccache riêng của Codex), không cấp cache của Claude.
  Lock file được mở read-only để `flock`; khả năng này được kiểm trong smoke test S0.
- `/tmp` mặc định ghi được trong sandbox Codex (cần cho runtime dir `/tmp/hvx-*` của nested).
- Push bị vô hiệu ở clone. Codex không bao giờ dùng `--dangerously-bypass-approvals-and-sandbox`.

**Auto-review cho escalation (DV3-14, user duyệt 2026-10-11; áp dụng từ WO-04, WO-01 giữ quy trình cũ).**
Codex vẫn chạy `-s workspace-write -a on-request`. Work order nào ghi rõ "Auto-review: allowed" thì được khởi động thêm
`-c 'approvals_reviewer="auto_review"'`: escalation do reviewer tự động của Codex duyệt thay user. Phạm vi:

| Loại thao tác | Duyệt |
|---|---|
| `git commit` / `fetch` / `merge` trong clone Codex (không push, không rebase/force); tải source đã cho phép (github.com/hyprwm, đúng tag/commit trong script); `systemd-run --user --scope` và build dưới `heavy.lock`; bind socket UNIX cho unit test; lệnh hệ thống khác chỉ khi work order liệt kê | Auto-review |
| Khởi động hoặc điều khiển compositor nested (`scripts/nested.sh start/run/dev/capture`, `hyprctl`, `grim`, test client, `/dev/dri`), hoặc bất cứ thứ gì chạm socket Wayland/Hyprland của host | **User duyệt tay**: chạy trong phiên Codex không có `approvals_reviewer` (ví dụ T015–T017 live, T022, T033/T034) |
| Ghi ngoài clone, `~/.cache/hyprvx/codex` và `/tmp`; `~/.config`, Caelestia, `/usr`, sudo, đổi cấu hình hệ thống | Cấm (không đổi) |

- Không bao giờ bật Full Access (`danger-full-access`) hay Bypass (`--dangerously-bypass-approvals-and-sandbox`).
- Reviewer từ chối ⇒ mục đó BLOCKED và báo lại; không tìm đường vòng.
- Mỗi commit thêm trailer `Escalations: <danh sách escalation đã được auto-review duyệt | none>`.
- Acceptance gate (GATE-A, GATE-N, T-ID, G1), stop condition và quy trình review/tích hợp của Claude (§O4) giữ nguyên.

```fish
# Phiên Auto-review (chỉ cho work order ghi "Auto-review: allowed"):
env HYPRVX_AGENT=codex codex -C /home/towfienes/Projects/HyprVX-v3-codex -s workspace-write -a on-request \
    -c 'approvals_reviewer="auto_review"' --add-dir ~/.cache/hyprvx/codex -m <model>
```

### O4. Handoff và tích hợp
1. Codex commit nhỏ theo task trên `v3/001-codex` với trailer `Task:`, `Verify:`, `Result:`, `Evidence:`, `Not-run:`.
2. Claude lấy commit **từ bên ngoài clone** và không bao giờ chạy lệnh git bên trong clone (tránh hook/config của
   clone): `git fetch /home/towfienes/Projects/HyprVX-v3-codex v3/001-codex:refs/codex/v3/001-codex`.
3. Claude review diff (`git diff v3/001-native-spatial-camera...refs/codex/v3/001-codex`), import evidence của
   run-id trong trailer vào `~/.local/share/hyprvx/evidence/codex/<run-id>/` và chạy `sha256sum -c SHA256SUMS`
   (thiếu file hoặc lệch checksum ⇒ từ chối handoff, kết quả coi là NOT RUN), chạy lại lệnh verify, rồi
   `git merge --no-ff refs/codex/v3/001-codex` vào `v3/001-native-spatial-camera`, tick `tasks.md`, ghi `integration-log.md`.
4. Acceptance fail thì không merge. Codex lấy cập nhật bằng `git fetch origin` + `git merge` (không rebase/force).
5. Phụ thuộc chưa xong: code theo contract đã freeze, không theo HEAD dở của bên kia; bị chặn thì ghi BLOCKED và
   chuyển task. Không ai `stash`, `reset --hard`, `clean` trên checkout của bên kia. Mỗi lần tích hợp Claude kiểm
   `git rev-parse dev` == `15f75b0`.

### O5. Tài nguyên máy
Một job nặng tại một thời điểm (build Hyprland hoặc nested session) qua `flock ~/.cache/hyprvx/locks/heavy.lock`;
`MemAvailable ≥ 6.5 GiB` trước khi build; dừng khi PSI memory full avg10 > 10; `-j4`, `MemoryMax=6G`. Job nhẹ chạy
song song. Chi tiết: `specs/001-native-spatial-camera/contracts/test-tooling.md` §2.

### O6. Model và quota
Claude Opus 5.5 giữ một phiên dài cho lane compositor, không đổi model giữa phiên; mở phiên mới ở ranh giới cổng
với handoff `.ai/`. Codex: một phiên bền cho lane hạ tầng (`codex resume --last`); model mạnh hơn chỉ dùng cho review
cổng. Chọn model theo kết quả kiểm chứng trên dự án, không theo tên model.
