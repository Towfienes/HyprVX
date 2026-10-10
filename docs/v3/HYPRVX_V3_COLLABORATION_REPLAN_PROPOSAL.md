# HyprVX V3 — Đề xuất điều chỉnh kế hoạch thực thi cho Claude Code + Codex

**Ngày:** 2026-10-10
**Baseline:** `v3/spec-foundation@dadcc95` (clean). Worktree V1 `HyprVX` ở `dev@15f75b0`, không đổi.
**Tác giả:** Claude Code (Principal Architect / Integration Owner).
**Trạng thái:** **ĐỀ XUẤT, chưa có hiệu lực.** Tài liệu này không thay thế [master plan](HYPRVX_V3_MASTER_PLAN.md),
[constitution](../../.specify/memory/constitution.md) hay [Feature 001](../../specs/001-native-spatial-camera/spec.md).
Nó không phải `tasks.md` và không phê duyệt Sprint 1. Mọi thay đổi ở mục J chỉ được áp dụng sau khi user duyệt.

**Đã đọc:** master plan (đủ 655 dòng), constitution, `spec.md`, `plan.md`, `research.md`, `data-model.md`,
`quickstart.md`, `contracts/transform-contract.md`, `contracts/dev-control.md`, `checklists/verification.md`,
[Codex review](CODEX_REVIEW_AND_CONTRIBUTION_PLAN.md) (có trong repo, commit `dadcc95`), `AGENTS.md`, `.ai/NEXT_TASK.md`.

**Đã kiểm tra trực tiếp (read-only):** `CMakeLists.txt`, `core/src/Camera.cpp`, `core/include/hyprvx/core/Camera.hpp`,
format task của Spec Kit (`.claude/skills/speckit-tasks`, `speckit-implement`), Codex CLI 0.161.0 (`codex --help`),
RAM hiện tại, git branches/worktrees. Có chạy một phép tính số thực nhỏ bằng `python3 -I`.

**Chưa thực hiện:** build, test suite, compositor, benchmark, đo tốc độ agent. Mọi con số thời gian bên dưới là
**ước lượng**, không phải số đo. JEV advisor không kết nối được (`CONNECTION_CLOSED`) nên quyết định dựa trên phân
tích ở đây.

---

## A. Khuyến nghị tổng quát

**Chọn phương án Hybrid: Codex chỉ được bật cho những phase có lane độc lập thật sự.**

- **Claude giữ:** contract, toàn bộ patch compositor, debug compositor, tài liệu có thẩm quyền, `tasks.md`, tích hợp.
- **Codex nhận:** lane hạ tầng build và verification trong Sprint 1, review độc lập ở các cổng, và các lane logic thuần
  ở phase sau (quyết định lại mỗi feature).
- **Không** để hai agent cùng làm một subsystem, không chia patch series, không ép Codex phải luôn có việc.

**Lý do:**

1. **Có song song thật, nhưng có giới hạn.** Khoảng 7.5 WU hạ tầng (CMake, script build, harness, testclient, analyzer,
   kịch bản test native) độc lập với patch 0001–0005. Chạy chồng hai lane rút Sprint 1 từ khoảng 14–20 WU xuống
   khoảng 11–14.5 WU, tức **nhanh hơn 1.25–1.4×** (tốt nhất khoảng 1.5×, xấu nhất khoảng 1.05×). Không phải 2×,
   vì đuôi debug compositor là tuần tự.
2. **Giá trị lớn nhất là verification độc lập, không phải tốc độ.** V2 thất bại vì cùng một bên viết code và viết test
   ("14/14 PASS" không có pixel check). Tách *người viết patch* (Claude) khỏi *người viết test và oracle* (Codex) xử lý
   đúng lỗi gốc đó, và phù hợp với Constitution II.
3. **Quota.** Chuyển lane hạ tầng sang Codex giảm ước lượng 35–45% lượng dùng Claude Opus trong Sprint 1, đổi lại là
   chi phí review diff. Ước lượng này chưa được đo.

**Khi nào không nên dùng Codex:** debug compositor, patch render/input, quyết định contract, và Phase 4 (trackpad trên
phần cứng, cần người quan sát). Ở những phần này một agent với context liên tục cho kết quả tốt hơn.

`Decision: Hybrid theo phase, vì lane hạ tầng/verification độc lập thật và tạo verification độc lập; lane compositor
giữ một chủ. Rejected: Claude-only (chậm hơn khoảng 1.3× và tự review); Codex chỉ review (bỏ phí lane song song);
phân công A–E của Codex nguyên trạng (gói B cho Codex vừa viết vừa test CameraMath, và bắt Claude chờ API nằm trên
critical path).`

---

## B. Quyết định cho từng điểm trong review của Codex

Phân loại: **Confirmed** (đã xác nhận bằng source hoặc phép tính), **Plausible** (cần kiểm chứng), **Useful**,
**Covered** (đã có), **Incorrect**, **Unnecessary**, **Out of scope**.

| # | Điểm | Phân loại | Quyết định | Bằng chứng và lý do |
|---|---|---|---|---|
| 2.1 | T-STALE thiếu ca partial damage khi camera đứng yên | Confirmed | **ACCEPT** (Sprint 1, cần sửa spec) | FR-005 buộc camera đổi ⇒ full-output damage, nên 50 trạng thái camera ngẫu nhiên không bao giờ đi qua đường partial damage. `research.md` R2: `damageBox`/`damageRegion` chưa transform và decorations dùng chúng (`CHyprBorderDecoration.cpp:147`…). Lớp lỗi này chỉ lộ ra khi camera tĩnh và client tự damage. Thêm **T-STALE-STATIC**, giữ ca camera động. |
| 2.2 | Capture tất định (scene, thời điểm chụp, cursor, animation) | Useful, một phần đã có | **ACCEPT** trong contract test-tooling, không sửa spec | SC-001 đòi ×3 nhưng quickstart không có tín hiệu "frame sẵn sàng". Frame callback chỉ gửi khi surface được vẽ (`Compositor.cpp:795`, research R3), nên dùng nó làm ready-signal. Thêm: animations tắt, cursor ẩn khi chụp, config nested giống hệt nhau giữa stock và patched, và **precheck stock-vs-stock ×3** trước khi phán T-ID. Không nới ngưỡng T-ID. |
| 2.3 | Zoom 2× không vừa output | Confirmed | **ACCEPT có sửa** | 800×600 × 2 = 1600×1200 > output 1280×800 (`quickstart.md`), nên US2.2 không thể thỏa. Sửa: ca z=2 dùng cửa sổ 400×300 (thành 800×600 vừa output). Không đổi output size vì cửa sổ nested phụ thuộc màn hình host. Ca clipping ở mép riêng (T-EDGE): **DEFER** sang Phase 2; edge case culling đã có trong spec. |
| 2.4a | Identity chính xác (`forward(W) = W`) | Confirmed | **ACCEPT** | `python3 -I`: `1920 + (0.1 − 1920)·1 = 0.09999999999990905 ≠ 0.1`. Invariant 4 không thể đúng nếu không có nhánh riêng. Sửa: `isIdentity` ⇒ helper trả nguyên input. Compositor vẫn đi stock path khi identity (FR-002 đã yêu cầu). |
| 2.4b | Tương thích API Camera V1 | Confirmed | **ACCEPT** | `core/src/Camera.cpp:41-57` dùng `(w − P)·z`, viewport-local, không có `M`. Master plan C1 viết "thay `Camera.cpp`". Diễn giải đúng: `CameraMath` nhận `M` tường minh, `Camera.cpp` **delegate** với `M = 0`. Như vậy chỉ có một phương trình (Constitution IV), simulator và 4 test camera V1 giữ nguyên ngữ nghĩa. `z_min`/`z_max` là tham số, vì V1 clamp 0.1–4.0 còn MVP là 0.1–2.0. |
| 2.5 | Build headless | Confirmed | **ACCEPT** | `CMakeLists.txt:161-211`: SDL3, wayland-client, xkbcommon, hyprland REQUIRED vô điều kiện; đường dẫn tuyệt đối `/usr/share/wayland-protocols` ở 193-194. Quickstart dùng `-DHYPRVX_WITH_SDL=OFF`, option này **chưa tồn tại**. Master plan §L chỉ nêu SDL và plugin, thiếu gating cho `hyprvx_wayland_platform`/`hyprvx_native_dev`. Sửa: ba option `HYPRVX_WITH_SDL`, `HYPRVX_WITH_WAYLAND_PLATFORM`, `HYPRVX_WITH_HYPRLAND_PLUGIN` cùng `pkg_get_variable(wayland-protocols pkgdatadir)`. Headless vẫn cần GTest, nlohmann_json (dòng 69, nested guard) và Python3. |
| 2.6 | Phạm vi drag mâu thuẫn ownership | Confirmed | **ACCEPT, có giải pháp** (cần sửa spec/plan) | `plan.md` Phase 2 có patch 0008 "drag (begin/update/move)", trong khi Feature 001 có `O ≡ 0` và constitution V cấm sửa `L(w)`. Master plan C4: move trong Drift sửa `O(w)`, và controller cho việc đó thuộc Feature 002. Giải pháp: 001 giữ **tọa độ held-button/drag vào client** (seat-local, 0007) và **chọn deco/góc theo inverse** (`checkInputOnDecos`). **Move/resize bằng drag khi camera ≠ identity bị chặn** và ghi rõ. Move bằng `O(w)` chuyển sang Feature 002. |
| 2.7 | Thử unified world sớm hơn | Plausible, giá trị cao | **ACCEPT có sửa** (cần sửa roadmap) | Phase U phụ thuộc 3 và 7 (master plan §H), trong khi U3 là rủi ro sản phẩm lớn nhất. Phương án dự phòng (dồn cửa sổ vào "drift workspace" vật lý) thay đổi kiến trúc và cần duyệt riêng. Thêm **spike U0**: giới hạn thời gian, chỉ thu bằng chứng, không merge, chạy **sau cổng Feature 001**, trước hoặc đầu Phase 3. **Không** đưa vào Sprint 1 hay Feature 001. Milestone U và U-1..U-6 giữ nguyên. |
| 2.8a | Provenance và khả năng tái lập | Phần lớn Covered | **ACCEPT thuật ngữ** | FR-010, Constitution X và master plan §L đã có `build-info.json` (upstream, patchset_id, binary sha256, lib versions, compiler); patchset-id băm theo thứ tự patch. Thêm: phân biệt 3 mức **input / test-result / binary**. Binary sha256 được phép khác (§L đã nói vậy). |
| 2.8b | Constitution Check là đánh giá thiết kế | Useful | **ACCEPT wording** | Ghi rõ "design-time assessment" ở đầu bảng Constitution Check trong `plan.md`. |
| 2.8c | Quyết định license trước khi vendor | Confirmed, review **đánh giá thấp** | **ACCEPT, nâng thành blocker Sprint 1** | Patch **0001** (thuộc Sprint 1) vendor `CameraMath.hpp` vào Hyprland (BSD-3), trong khi license vẫn đang mở (master plan J, constitution TODO). Đề xuất: SPDX `BSD-3-Clause` cho riêng `CameraMath.hpp`; license toàn dự án quyết sau. |
| §3 | Phân công A–E | Một phần | **MODIFY** (mục D) | A: giữ cho Codex. B: tách, Claude viết header (là interface của mọi seam, nằm trên critical path), Codex viết test với oracle độc lập. C: giữ, thêm kịch bản T-* và U1. D: Claude. E (spike phân phối): **DEFER**, chọn owner khi bắt đầu Phase 2, mặc định Claude. |
| §4 | Model | Plausible, chưa xác minh | **DEFER** | Tên model GPT chưa được Claude xác minh. Dùng làm điểm khởi đầu và đánh giá theo kết quả thực tế. Không đổi default model hay settings. |
| §5 | Tiêu chí bàn giao | Đa số Covered | **ACCEPT** oracle độc lập và ≥10⁴ mẫu có seed; **REJECT** kéo pointer, popup, layer vào Sprint 1 | V-07 (harness từ chối target sai) và V-15 (scale 1.25) đã có. Pointer ≥ 20 điểm và popup thuộc Phase 2 (US3, V-16..V-19). |

**Phát hiện thêm của Claude (ngoài review Codex):**

- **K-1.** `tools/vpointer` nằm trong "test infra" của §I nhưng input không thuộc Sprint 1, nên vpointer **không gating
  Sprint 1**.
- **K-2.** Constitution ghi "Commits happen only when the user asks", nhưng handoff qua Git cần commit. Cần **ủy quyền
  commit thường trực trên `v3/001-*`**; không cần amendment constitution.
- **K-3.** Hai agent build stock, V2 và patched song song trên **một** cây source Hyprland sẽ ghi đè lẫn nhau. Cần mỗi
  variant một git worktree upstream riêng.

---

## C. Chiến lược thực thi điều chỉnh

### C1. Không đổi

Kiến trúc Strategy C, phương trình C1, Option A → B, thứ tự phase 0 → 1 → 2 → 3 → 4, các acceptance gate, stop condition,
ràng buộc host safety, và cổng spike phân phối ở cuối Phase 2.

### C2. Đổi trình tự trong Sprint 1

- **Hiện tại** (`NEXT_TASK.md` bước 4–5): "Test infrastructure first", sau đó mới tới patch, hoàn toàn tuần tự.
- **Đề xuất: "infra-before-claims".** Patch được *viết* song song với tool. Nhưng **không đánh giá hoặc claim bất kỳ
  patch nào** trước khi tool đã sẵn sàng và baseline stock đã được chứng minh tất định. Nguyên tắc chống "14/14 PASS"
  vẫn giữ nguyên; chỉ bỏ phần chờ không cần thiết.

### C3. Checkpoint mới

| Checkpoint | Vị trí | Nội dung |
|---|---|---|
| **K0** contract freeze | Đầu Sprint 1, trước khi giao việc | transform-contract (identity, V1 compat, signatures), `contracts/test-tooling.md` mới, spec changes P1–P3 |
| **GATE-A** baseline tất định | Trước khi phán T-ID | stock vs stock ×3 pixel-identical, cùng config nested |
| **GATE-N** negative control | Cùng lúc U1 | Analyzer **phải** phát hiện artifact trên V2-as-is; nếu không phát hiện thì analyzer bị coi là hỏng |
| **U0** spike unified world | Sau cổng Feature 001 | Giới hạn thời gian, chỉ bằng chứng, không merge |

### C4. Từ Phase 2 trở đi

Giữ nguyên thứ tự. Owner được quyết lại tại mỗi lần chạy `/speckit-tasks` của từng feature, dựa trên lane độc lập thực
tế (xem F3).

---

## D. Ma trận ownership (Sprint 1 = Phase 0 + 1)

ID ở đây là ID đề xuất; ID thật do `/speckit-tasks` sinh. **Integrator của mọi task là Claude.**

| ID | Owner | Deliverable | Phụ thuộc | File / subsystem | Contract | Verify | Song song với |
|---|---|---|---|---|---|---|---|
| K0 | Claude | Contract freeze + spec changes đã duyệt | Duyệt mục J | `specs/001-*/contracts/`, `spec.md`, `plan.md` | — | `/speckit-analyze` không còn CRITICAL | — (cổng) |
| C1 | Claude | `CameraMath.hpp` (header-only, SPDX) + `Camera.cpp` delegate M=0 | K0, license | `core/include/hyprvx/core/CameraMath.hpp`, `core/src/Camera.cpp` | transform-contract | 4 test camera V1 + simulator build không đổi | I1, I2 |
| C2 | Claude | Patch `0001`→`0005` | C1, upstream checkout | `patches/hyprland/` | transform-contract, dev-control | `git am` sạch; grep cấm `/ zoom` inline | lane Codex |
| C3 | Claude | Chạy T-*, debug, sửa | C2, I7, GATE-A | `patches/hyprland/` | — | Evidence trong `evidence/<run-id>/` | R1 |
| I1 | Codex | CMake headless + `build-v3/` GTest | K0 | `CMakeLists.txt` (độc quyền) | plan §L đã sửa | V-05; configure không cần SDL/Wayland/Hyprland; cấu hình đầy đủ vẫn build | C1, I2 |
| I2 | Codex | `scripts/fetch-hyprland.sh`, `apply-patches.sh`, `build-hyprland.sh`; scanner v0.4.6; `build-info.json`; worktree upstream mỗi variant; lock | K0 | `scripts/` | test-tooling (lock, layout) | V-01, V-03; V-02/V-04 thử với series rỗng hoặc V2 trước | C1, I1 |
| I3 | Codex | `test_camera_math.cpp`: invariant 1–6, ≥10⁴ mẫu có seed, oracle độc lập (long double hoặc giá trị dyadic tính tay) | C1 (header), I1 | `tests/unit/test_camera_math.cpp` + đăng ký CMake | transform-contract | V-06 | I4–I6 |
| I4 | Codex | Port harness: `--binary` prefix, re-verify mỗi lệnh, `recover`, `HYPRLAND_NO_SD_VARS=1`, `dbus-run-session`; `scripts/nested.sh` | I2 | `tools/hyprvx_nested.py`, `scripts/nested.sh` | test-tooling, Constitution I | V-07 (negative tests) | C2 |
| I5 | Codex | `tools/testclient`: nội dung tất định, marker, grid, lệnh cập nhật vùng nhỏ (cho T-STALE-STATIC), JSONL kèm sự kiện frame-ready; chế độ layer-surface tùy chọn (V-14) | K0 | `tools/testclient/` | test-tooling | Unit + smoke trong nested stock | C2 |
| I6 | Codex | `tools/analyze_frame.py` (`python -I`): bbox, marker, scale, displacement, diff với ảnh tham chiếu, output JSON | K0 | `tools/analyze_frame.py` + test ảnh tổng hợp | test-tooling, tolerance của contract | Test ảnh tổng hợp có đáp án | C2 |
| I7 | Codex | `tests/native/`: T-ID, T-Z025/05/2, T-PAN, T-STALE, T-STALE-STATIC, T-OFF, viết **từ spec, trước khi có patch** | I4–I6 | `tests/native/` | spec US1/US2/US4, dev-control | Dry-run trên stock | C2 |
| I8 | Codex | GATE-A (stock ×3) + U1 với V2-as-is (V-08, cũng là GATE-N) | I2, I4–I7 | `evidence/` | — | Ảnh + JSON + build-info | C2 |
| I9 | Codex | `tools/vpointer` (**không gating** Sprint 1) | I4 | `tools/vpointer/` | test-tooling | Smoke trong nested | Bất kỳ |
| R1 | Codex | Chạy lại suite độc lập trên commit Claude bàn giao + `codex review` patch series | C3 | — (chỉ đọc + evidence) | Constitution II/IV | Báo cáo PASS/FAIL kèm artifact | — |
| G1 | Claude | Cổng Sprint 1: `verification.md`, `.ai/CURRENT_STATE.md`, `NEXT_TASK.md` | Tất cả | `specs/`, `.ai/` | — | Mọi V-xx thuộc Sprint 1 có artifact | — |

**File độc quyền** (không file nào thuộc cả hai bên):

- **Codex:** `CMakeLists.txt`, `scripts/`, `tools/`, `tests/native/`, `tests/unit/test_camera_math.cpp`.
- **Claude:** `patches/`, `CameraMath.hpp`, `Camera.cpp`, `specs/` (gồm `tasks.md`, `verification.md`,
  `integration-log.md`), `docs/`, `.ai/`, `.specify/`, `AGENTS.md`, `CLAUDE.md`.
- Chuyển quyền sở hữu (ví dụ Claude cần sửa CMake sau Sprint 1) phải ghi vào `integration-log.md`.
- **Claude không sửa test của Codex; Codex không sửa patch của Claude.** Bên nào thấy lỗi ở phần của bên kia thì báo
  lại qua log.

---

## E. Đồ thị phụ thuộc và critical path

```text
                ┌─► C1 CameraMath (Claude) ──► C2 patch 0001→0005 (Claude, tuần tự) ───────────┐
K0 contract ────┤                     └──────► I3 test CameraMath (Codex)                      │
freeze (Claude) ├─► I1 CMake headless (Codex) ──┘                                              ├─► lần chạy patched đầu tiên
                └─► I2 script + build stock (Codex) ─► I4 harness ─► I5 testclient ─┐          │     │
                                                                     I6 analyzer ───┴─► I7 T-* ─► I8 GATE-A + U1/GATE-N ─┘
                                                     I9 vpointer (không gating)              │
                                                                                             ▼
                                         C3 debug loop (Claude, tuần tự) ◄──► R1 chạy lại độc lập + review (Codex)
                                                                                             │
                                                                                             ▼
                                                                                    G1 cổng Sprint 1
```

- **Critical path** = max(lane Codex I2→I8 khoảng 6–7 WU; lane Claude K0+C1+C2 khoảng 4–7 WU) + đuôi C3 khoảng 2–5 WU
  (tuần tự) + G1.
- **Song song thật:** toàn bộ lane I1–I9 chạy cùng lúc với C1–C2.
- **Phải tuần tự:** K0 trước khi giao việc; GATE-A trước khi phán T-ID; T-ID PASS trước khi phán T-Z* (stop condition);
  cổng Sprint 1 trước mọi task Phase 2; cổng Phase 2 (L4 + L5 + L5b + báo cáo spike) trước Feature 002; U0 sau cổng 001.
- **Rủi ro phát hiện sớm hơn mà không mở rộng scope:** baseline tất định (GATE-A), analyzer hỏng (GATE-N), license
  (trước 0001), mâu thuẫn drag (K0), và tính khả thi unified world (U0 trước khi đầu tư Phase 4–5).

---

## F. So sánh workflow

**Đơn vị:** WU (work unit) ≈ một ngày-phiên agent làm tập trung. Độ tin cậy thấp; dùng để so sánh tương đối, không phải
lịch.

### F1. Sprint 1

| Hạng mục | WU | Claude-only | Hybrid |
|---|---|---|---|
| K0 contract + spec | 0.5 | Claude | Claude |
| C1 CameraMath | 0.5 | Claude | Claude |
| I1 CMake | 0.5 | Claude | Codex |
| I2 script/build/provenance | 1.0 | Claude | Codex |
| I3 test CameraMath | 0.5 | Claude | Codex |
| I4 harness | 1.5 | Claude | Codex |
| I5 testclient | 1.0 | Claude | Codex |
| I6 analyzer | 1.0 | Claude | Codex |
| I7 kịch bản T-* | 1.0 | Claude | Codex |
| I8 GATE-A + U1 | 0.5 | Claude | Codex |
| I9 vpointer | 0.5 | Claude | Codex |
| C2 patch 0001–0005 | 3–6 | Claude | Claude |
| C3 debug | 2–5 | Claude | Claude (+ R1 Codex ~0.5) |
| Review và tích hợp Codex | — | 0 | Claude ~1.0 |
| G1 | 0.5 | Claude | Claude |
| **Thời gian thực (critical path)** | | **14–20** | **11–14.5** |

**Kịch bản:**

- **Tốt (≈1.5×):** patch ít seam ẩn; tool của Codex sẵn sàng đúng lúc patch xong.
- **Trung bình (≈1.25–1.4×):** debug C3 chiếm phần lớn thời gian.
- **Xấu (≈1.05×):** harness hoặc sandbox Codex gặp trục trặc, handoff phải làm lại, hoặc C3 kéo dài. Gần như toàn bộ lợi
  thế biến mất.

**Chi phí của Hybrid:** review diff (khoảng 1 WU Claude), cold-start Codex (đọc danh sách trong `AGENTS.md` một lần mỗi
phiên), tranh chấp build/RAM, viết contract test-tooling (đằng nào cũng cần).

### F2. Đánh giá định tính

| Tiêu chí | Claude-only | Hybrid |
|---|---|---|
| Context liên tục | Tốt nhất | Tốt ở lane compositor; lane infra do Codex giữ context riêng |
| Tự review / điểm mù | Rủi ro cao (giống V2) | Tách patch và test, có review chéo ở cổng |
| Khám phá repo lặp lại | Thấp | Trung bình; giảm bằng brief trỏ đúng mục research |
| Xung đột merge | Không có | Thấp nếu giữ file độc quyền |
| Usage Claude | 100% | ≈55–65% (ước lượng) |
| Tranh chấp RAM | Thấp | Cần lock (mục I) |

### F3. Các phase sau (ước lượng, quyết lại ở từng feature)

| Phase | Lane cho Codex | Speedup kỳ vọng |
|---|---|---|
| 2 Input + spike | vpointer, kịch bản L5b, chế độ popup của testclient, V-19 | ≈1.1–1.2× |
| 3 Multi-window | Logic thuần trong core (WindowWorldState store, mode FSM + tests); Claude giữ seam API/mechanism | ≈1.2–1.4× |
| 4 Trackpad | Rất ít (unit test arbitration); cần phần cứng và người | ≈1.0× |
| 5 Snap/cluster | Logic core (V1 Island) + L2; Claude tích hợp | ≈1.3–1.6× |
| 6 Caelestia | Không | 1.0× |
| 7 Performance | `presentation-probe`, script benchmark; Claude làm camera-aware simplify | ≈1.3× |
| U | Script U-4 (so `hyprctl clients -j` nested); Claude làm compositor | ≈1.1× |

---

## G. Tích hợp với Spec Kit (vẫn là workflow duy nhất)

- **Không tạo hệ thống task thứ hai.** Owner ghi ở **cuối description** của task, đúng format bắt buộc:
  `- [ ] T014 [P] Implement deterministic test client in tools/testclient/ (owner: codex)`.
- `[P]` chỉ dùng khi task thật sự độc lập (khác file, không phụ thuộc task chưa xong). Không ép mọi task song song.
- **Chỉ Claude sửa `tasks.md`** (sinh, tick `[X]`, sửa). Codex không tick task, tránh conflict trên file này.
- **`/speckit-tasks`:** sinh cho **toàn Feature 001**, có ranh giới rõ "Sprint 1 (Phase 0+1)" và "Phase 2 — BLOCKED
  tới cổng Sprint 1", có gate task (K0, GATE-A, GATE-N, G1) và review task (R1). Argument đề xuất:
  > "Tag mỗi task `(owner: claude|codex)` theo ma trận ownership trong
  > `docs/v3/HYPRVX_V3_COLLABORATION_REPLAN_PROPOSAL.md` §D; phân tách Sprint 1 và Phase 2; thêm gate tasks;
  > `[P]` chỉ khi khác file và không phụ thuộc."
- **`/speckit-analyze`:** kiểm tra nhất quán spec/plan/tasks với constitution. CRITICAL phải được sửa trước khi
  implement. Kiểm thêm: không task nào có hai owner; không task Phase 2 nằm trong Sprint 1.
- **`/speckit-implement`:** Claude chạy với filter `"owner: claude, Sprint 1 only"`. Codex **không** chạy skill này
  (skill nằm ở `.claude/skills`). Codex nhận **work order** gồm danh sách task ID, contract liên quan, lệnh verify và
  ranh giới file.
- **Handoff bền vững:** `specs/001-native-spatial-camera/integration-log.md` (append-only, Claude ghi), mỗi dòng gồm
  task ID, `branch@sha`, lệnh verify, kết quả, artifact, verdict review. Cùng với commit trailer và `.ai/`, đây là toàn
  bộ trạng thái chia sẻ.

---

## H. Đề xuất vận hành Sprint 1

### H1. Worktree và branch

```fish
# 0. (sau khi user duyệt mục J) áp các thay đổi đã duyệt trên v3/spec-foundation và commit theo yêu cầu user

# 1. Worktree tích hợp của Claude = HyprVX-v3 hiện tại
cd /home/towfienes/Projects/HyprVX-v3
git switch -c v3/001-native-spatial-camera            # tách từ v3/spec-foundation

# 2. Sau khi tasks.md + contracts (K0) đã commit trên branch tích hợp:
git worktree add -b v3/001-codex ../HyprVX-v3-codex v3/001-native-spatial-camera
git worktree list                                     # kỳ vọng: HyprVX [dev], HyprVX-v3, HyprVX-v3-codex

# 3. User tự mở Codex (Claude không sửa config Codex). --add-dir cần cho build cache và để commit trong worktree
#    (git object nằm ở repo chung HyprVX/.git). Codex sẽ hỏi trust thư mục mới.
codex -C /home/towfienes/Projects/HyprVX-v3-codex -s workspace-write -a on-request \
      --add-dir ~/.cache/hyprvx --add-dir /home/towfienes/Projects/HyprVX/.git -m <model>
```

| Worktree | Branch | Chủ | Ghi chú |
|---|---|---|---|
| `HyprVX` | `dev` | — | V1, **đóng băng**, không agent nào động vào |
| `HyprVX-v3` | `v3/001-native-spatial-camera` | Claude | Tích hợp, patch, docs/specs |
| `HyprVX-v3-codex` | `v3/001-codex` | Codex | Lane I |
| `~/.cache/hyprvx/src/hyprland-efb5099` | pristine | — | Chỉ đọc; git worktree con: `stock` và `v2` (Codex), `v3-dev` (Claude) |

### H2. Quy tắc handoff

1. Codex commit nhỏ theo task trên `v3/001-codex`, mỗi commit có trailer:
   ```text
   Task: T014
   Verify: <lệnh tái lập>
   Result: PASS | FAIL | BLOCKED | NOT RUN (kèm lý do)
   Evidence: evidence/<run-id>/
   Not-run: <phần chưa kiểm>
   ```
2. Claude chạy `git log/diff v3/001-native-spatial-camera..v3/001-codex`, review diff, chạy lại lệnh verify (nhẹ thì
   luôn chạy; nặng thì chạy dưới lock khi liên quan), rồi `git merge --no-ff`, tick `tasks.md`, ghi `integration-log.md`.
3. **Acceptance fail thì không merge.** Codex sửa trên branch của mình.
4. Codex lấy cập nhật bằng `git merge v3/001-native-spatial-camera`. Không rebase, không force-push.
5. Claude giao bản patch để kiểm chứng độc lập bằng một commit cụ thể có ghi trong log. Codex chạy R1 trên đúng commit
   đó, hoặc dùng `codex review` trên range commit tương ứng.

### H3. Phụ thuộc vào việc bên kia chưa xong

- Code theo **contract đã freeze**, không theo HEAD đang dở của bên kia.
- Chỉ lấy commit **đã bàn giao** (có trong log), không cherry-pick việc đang làm.
- Bị chặn thì ghi `BLOCKED` và chuyển sang task độc lập khác. Không stub vào file của bên kia. Test double chỉ được đặt
  trong file của chính mình (ví dụ analyzer chạy trên ảnh tổng hợp, T-* dry-run trên stock).
- Cần đổi contract: chỉ Claude sửa, tăng dòng version trong contract, ghi log; Codex merge vào branch của mình.

### H4. Bảo vệ dữ liệu

- Không agent nào `git stash`, `reset --hard`, `clean`, hay `checkout --` trong worktree của bên kia. Cần cất việc dở thì
  dùng WIP commit.
- Mỗi lần tích hợp, Claude kiểm `git rev-parse dev` = `15f75b0` và worktree `HyprVX` vẫn như trước.
- Không động vào V2, host Hyprland hay Caelestia. Không push, không merge vào `v3/spec-foundation` khi chưa được duyệt.

### H5. Trình tự

1. **Ngày 0:** Claude làm K0 rồi C1. Codex song song làm I1 và I2 (bắt đầu build stock sớm vì build tốn thời gian thực).
2. Codex làm I4 → I5/I6 → I7 → I8. Claude làm C2.
3. **GATE-A + GATE-N** pass, sau đó chạy patched lần đầu.
4. Vòng C3 (Claude debug) và R1 (Codex chạy lại độc lập).
5. **G1.** Dừng để user duyệt trước Phase 2.

**Không** đưa input (0006–0009), popup, L5b hay spike phân phối vào Sprint 1.

### H6. Smoke test S0 (đầu Sprint 1)

Kiểm tra sandbox Codex (`workspace-write`) có cho phép `/dev/dri`, Wayland socket của nested và `systemd-run --user`
hay không. Nếu bị chặn, các bước nặng (build Hyprland, nested) sẽ do user chạy bằng `!` hoặc do Claude chạy. Kết quả
ghi vào log. **Không** dùng `--dangerously-bypass-approvals-and-sandbox`.

---

## I. Rủi ro và giảm thiểu

| Rủi ro | Giảm thiểu |
|---|---|
| Merge conflict | File độc quyền (D); `tasks.md`, specs và docs chỉ Claude sửa; merge `--no-ff` tuần tự |
| Interface không khớp | Contract-first (K0); contract có version; consumer code theo contract |
| Làm trùng việc | Một task ID một owner; work order ghi rõ ranh giới; integration log |
| Test bất đồng | Tái lập trên cùng commit + `build-info.json`; triage bằng stock (phải pass) và V2 (phải fail); Claude quyết; tolerance thuộc spec, **không ai được nới** |
| Test viết để "pass" | Test viết từ spec **trước** khi có patch; mọi chỉnh sửa test sau khi có patch phải ghi lý do trong log; Claude không sửa test của Codex |
| Mất context | Commit trailer, integration log, `.ai/CURRENT_STATE.md`; phiên Codex bền (`codex resume --last`) |
| Áp lực RAM / swap thrash | **Heavy lock** `flock ~/.cache/hyprvx/locks/heavy.lock` cho build Hyprland và nested session (mỗi lúc một job nặng); kiểm `MemAvailable ≥ 6.5 GiB` trước khi build Hyprland; dừng nếu `/proc/pressure/memory` báo full avg10 > 10; `-j4`, `MemoryMax=6G`; ccache dùng chung (an toàn khi chạy đồng thời); job nhẹ (core `-j2`, GTest, python) được chạy song song. RAM lúc lập kế hoạch: 7.3 GiB available, khác lúc audit là 2.8 GiB, nên phải kiểm lại mỗi lần |
| Hai nested session cùng lúc | Ban đầu cấm (thuộc heavy lock); chỉ nới khi đã đo RSS nested |
| Sandbox Codex chặn GPU/Wayland | S0; fallback do user hoặc Claude chạy; không bypass sandbox |
| Verification không đầy đủ | Thiếu môi trường thì ghi `NOT RUN`/`BLOCKED`, không bao giờ ghi PASS; Claude chạy lại trước khi tick |
| Ghi nhầm vào `.git` của V1 | Quy tắc chỉ ghi `v3/001-*`; Claude kiểm ref `dev` mỗi lần tích hợp |

---

## J. Đề xuất thay đổi tài liệu có thẩm quyền

Chưa áp dụng thay đổi nào. **Mọi mục đều cần user duyệt.** Sau khi duyệt, Claude áp lên `v3/spec-foundation`.

| ID | Tài liệu | Thay đổi | Mức |
|---|---|---|---|
| **P1** | `spec.md` US2.2, US2.3, SC-002 | Ca z=2 dùng cửa sổ 400×300 (hoặc "cửa sổ có kích thước sao cho box đã transform nằm gọn trong output"); thêm scenario camera tĩnh + client partial damage (T-STALE-STATIC); SC-002 thêm "0 mismatch ở ca tĩnh" | Duyệt (acceptance) |
| **P2** | `spec.md`, `plan.md` Phase 2 (0008) | 0008 thu hẹp thành chọn deco/góc theo inverse + tọa độ held-button; move/resize bằng drag khi camera ≠ identity bị chặn và ghi rõ; move bằng `O(w)` thêm vào phạm vi Feature 002 (master plan §K) | Duyệt (scope) |
| **P3** | `contracts/transform-contract.md` | Invariant 4: "`isIdentity` ⇒ trả nguyên input (short-circuit)"; mục "V1 compatibility: `Camera` delegate với M = 0"; `z_min`/`z_max` là tham số; signatures C++ | Duyệt (contract) |
| **P4** | `plan.md`, `quickstart.md`, master plan §L | Ba option CMake + `pkg_get_variable`; định nghĩa headless (cần GTest, nlohmann_json, Python3); bảng Constitution Check ghi "design-time assessment" | Duyệt (làm rõ) |
| **P5** | `contracts/test-tooling.md` (mới) | CLI và JSONL schema của testclient (gồm frame-ready, lệnh partial update), JSON output của analyzer, subcommand `nested.sh`, layout `evidence/<run-id>/`, quy tắc capture tất định, lock protocol | Duyệt (artifact mới) |
| **P6** | Master plan §H | Thêm hàng **U0**: spike unified world giới hạn thời gian, chỉ bằng chứng, sau cổng 001, trước hoặc đầu Phase 3; U giữ nguyên | Duyệt (roadmap) |
| **P7** | Master plan §I, `.ai/NEXT_TASK.md` | "Infra-before-claims"; hai lane; GATE-A và GATE-N; vpointer không gating; license là blocker trước 0001; thuật ngữ tái lập 3 mức | Duyệt (trình tự) |
| **P8** | Master plan (mục mới "O. Collaboration workflow") | Tóm tắt D, G, H, I; trỏ về tài liệu này | Duyệt |
| **P9** | Constitution | **Không đổi.** Commit dựa trên ủy quyền thường trực của user cho `v3/001-*` | Chỉ cần user xác nhận |
| **P10** | `CameraMath.hpp` + master plan §J | SPDX `BSD-3-Clause` cho riêng file này; license toàn dự án vẫn mở | Duyệt (license) |

**Diff mẫu (P3, invariant 4):**

```diff
-4. Identity ⇒ `forward(W) = W` exactly.
+4. Identity ⇒ `forward(W) = W` and `inverse(S) = S` exactly. Implementations MUST short-circuit on
+   `isIdentity(C, M)` and return the input unchanged (floating-point `M + (W − M)` is not exact).
+   The compositor additionally takes the stock path when identity (FR-002).
```

**Diff mẫu (P1, US2.2):**

```diff
-2. **Given** z ∈ {0.5, 2.0} and pans of ±300 px, **When** captured, **Then** size and displacement match the
-   contract within ± 2 px.
+2. **Given** z = 0.5 (800×600 window) or z = 2.0 (400×300 window, so the transformed box fits the 1280×800
+   output), and pans of ±300 px, **When** captured, **Then** size and displacement match the contract within ± 2 px.
+5. **Given** a fixed non-identity camera, **When** the client updates a small region in deterministic order,
+   **Then** each partial-damage frame matches a forced full-damage frame of the same client state.
```

---

## K. Approval cần từ user

1. Chiến lược **Hybrid** (mục A).
2. P1, P2, P3, P5 (spec và contract).
3. P6: thời điểm spike U0.
4. P10: license `BSD-3-Clause` cho `CameraMath.hpp` (blocker của patch 0001).
5. Ủy quyền **commit thường trực trên `v3/001-*`** (không push, không merge vào `v3/spec-foundation` khi chưa duyệt).
6. User tự chạy Codex với `--add-dir` như H1 (Claude không đổi config, auth hay model mặc định của Codex).
7. Duyệt Sprint 1 để chạy `/speckit-tasks`.

## L. Bước Spec Kit kế tiếp sau khi duyệt

1. Áp P1–P10 đã duyệt lên `v3/spec-foundation`, commit theo yêu cầu user.
2. `git switch -c v3/001-native-spatial-camera`.
3. `/speckit-tasks` với argument ở mục G.
4. `/speckit-analyze` và sửa hết CRITICAL.
5. **Dừng** để user duyệt `tasks.md`.
6. Sau đó tạo worktree Codex (H1), giao work order lane I, và Claude chạy `/speckit-implement` với filter
   `owner: claude, Sprint 1 only`.
