# HyprVX V3 — Đánh giá độc lập và kế hoạch Codex hỗ trợ Claude

**Ngày:** 2026-10-10

**Baseline được đánh giá:** `ace7cd4`, branch `v3/spec-foundation`.

**Trạng thái:** Đề xuất phối hợp; chưa triển khai. Người dùng chọn hướng **cùng làm + review**.

Tài liệu này ghi nhận đánh giá và đề xuất riêng của Codex. Nó không thay thế hoặc sửa master plan,
constitution, spec hay plan của Claude; không phải `tasks.md` và không tự phê duyệt Sprint 1.
Việc cho phép tạo tài liệu này chỉ cho phép ghi tài liệu, không cho phép bắt đầu implementation.

Tài liệu đã xem:
- [Master plan V3](HYPRVX_V3_MASTER_PLAN.md).
- [Constitution](../../.specify/memory/constitution.md).
- [Feature 001](../../specs/001-native-spatial-camera/spec.md), gồm plan, research, data model,
  contracts, quickstart và checklist verification đi kèm.

## 1. Đánh giá chung

**Plan của Claude hợp lý để tiếp tục phát triển.** Bốn lựa chọn nên giữ:

- Tái sử dụng core V1 và giữ patch V1/V2 làm tài liệu tham khảo.
- Tách cơ chế biến đổi cửa sổ trong Hyprland khỏi policy trong plugin.
- Yêu cầu render và input dùng chung một hợp đồng tọa độ.
- Kiểm chứng bằng ảnh và tọa độ pointer thật trong nested compositor.

Codex đồng ý giữ hướng kiến trúc này. Phần cần bổ sung chủ yếu là **thiết kế kiểm thử,
khả năng build độc lập và ranh giới công việc**. Các nguyên nhân RC1/RC2 trong research vẫn cần
kiểm chứng runtime; đọc source chưa đủ để kết luận bản sửa sẽ hoạt động.

Đánh giá này dựa trên tài liệu và đối chiếu có giới hạn với CMake, Camera V1 và nested guard/harness.
Không phải một cuộc audit lại toàn bộ source Hyprland hoặc toàn bộ patch V2.

## 2. Những điểm cần thống nhất trước khi triển khai

Các phương án bên dưới là đề xuất để Claude và người dùng xem xét, chưa phải thay đổi đã được áp dụng
vào bộ Spec Kit. Nếu khác spec/constitution hiện hành, phải thống nhất và cập nhật tài liệu có thẩm quyền
trước khi implement.

### 2.1. T-STALE cần kiểm tra partial damage khi camera đứng yên

Camera thay đổi vốn yêu cầu full-output damage. Chỉ đổi 50 trạng thái camera rồi so ảnh có thể bỏ sót lỗi
damage khi camera đứng yên.

Đề xuất bổ sung ca giữ camera khác identity, cho client cập nhật một vùng nhỏ, rồi so ảnh với lần redraw
toàn bộ của cùng trạng thái client. Các lần cập nhật phải có thứ tự xác định và có xác nhận frame sẵn sàng.
Giữ cả bài kiểm tra camera động hiện có.

**Bằng chứng:** `spec.md` US2.3, FR-005; master plan §E và §I.

### 2.2. Pixel-identical cần scene và thời điểm chụp ổn định

Chốt scene, kích thước, vị trí, seed ngẫu nhiên, trạng thái cursor và animation. Dùng testclient có nội dung
xác định cho baseline. Chụp sau sự kiện xác nhận client/frame sẵn sàng, tránh dựa vào thời gian ngủ cố định.
Mỗi ảnh phải gắn với camera revision, trạng thái client và build tương ứng.

Không nới ngưỡng T-ID để che sự không ổn định của harness. Nếu baseline stock không tự lặp lại được,
sửa harness trước khi đánh giá bản vá.

**Bằng chứng:** `spec.md` SC-001, SC-002; `checklists/verification.md` V-09..V-13.

### 2.3. Ca zoom 2× cần scene vừa output

Cửa sổ 800×600 thành 1600×1200 khi zoom 2×, lớn hơn output 1280×800 trong quickstart.
Không thể luôn đòi đủ bốn marker trong ảnh đó.

Đề xuất dùng scene/output đủ lớn cho bài kiểm tra toàn cửa sổ. Kiểm tra clipping ở biên bằng ca riêng,
với vùng nhìn thấy được xác định trước; không nhầm clipping đúng tại mép output với lỗi clipping surface.

**Bằng chứng:** `spec.md` US2; `quickstart.md` kích thước nested output và testclient.

### 2.4. Identity chính xác và khả năng tương thích Camera V1

Contract yêu cầu `forward(W) = W` chính xác ở identity. Phép tính số thực thông thường không tự bảo đảm
điều đó khi có monitor origin khác 0. Phép kiểm tra nhỏ trong phiên review cho kết quả:

```text
M = P = 1920.0, W = 0.1, z = 1.0
M + (W - P) * z = 0.09999999999990905
```

Đây là sai số số học rất nhỏ, không phải bằng chứng đã quan sát thấy lỗi pixel. Tuy nhiên, nó cho thấy
contract equality chính xác cần xử lý có chủ đích.

Đề xuất nhánh identity của helper trả nguyên đầu vào; compositor đi đường stock khi OFF.
Giữ nguyên ngữ nghĩa viewport-local của API Camera V1 khi đưa helper global-logical mới vào.
Không thay đổi âm thầm tọa độ đầu ra mà simulator và platform đang sử dụng.

**Bằng chứng:** `contracts/transform-contract.md` invariant 4;
`core/src/Camera.cpp` phương thức `Camera::worldToScreen`.

### 2.5. Build headless cần loại dependency native khỏi configure

CMake hiện tại yêu cầu SDL3, Wayland, XKB, Hyprland, scanner và sinh protocol.
Chỉ đề cập hai cờ tắt SDL/plugin chưa định nghĩa đủ cách tắt phần Wayland platform.

Đề xuất gói build headless phải configure/build/test core và runtime trong môi trường không có các
dependency đồ họa trên. Mỗi native target và dependency discovery tương ứng chỉ được bật khi xây phần đó.
Dùng hệ thống CMake hiện có, không tạo build system thứ hai.

**Bằng chứng:** `CMakeLists.txt:161-211`; master plan §L; `quickstart.md` lệnh core-only dự kiến.

### 2.6. Phạm vi drag cần thống nhất với mô hình ownership

Feature 001 giữ `O(w)=0`, không cho Drift sửa logical geometry, nhưng plan Phase 2 có patch drag/move.
Cần xác định thao tác nào chỉ chuyển đổi tọa độ input và thao tác nào thực sự di chuyển cửa sổ.

Đề xuất giữ Feature 001 tập trung render, hit-test và pointer delivery; thao tác di chuyển cửa sổ bằng
world offset thuộc Feature 002. Không bỏ qua vấn đề drag: ghi rõ việc chuyển phạm vi này để Claude
thống nhất trước khi cập nhật spec/plan. Tránh triển khai drag bằng cách sửa logical geometry chỉ vì
world-offset controller chưa tồn tại.

**Bằng chứng:** `plan.md` Phase 2; `data-model.md` WindowWorldState; constitution nguyên tắc V.

### 2.7. Thử tính khả thi của unified world sớm hơn

Unified world là yêu cầu sản phẩm cuối bắt buộc, nhưng PoC hiện nằm trong phase phụ thuộc Phase 3 và 7.
Sau khi render/input cơ bản đạt, nên thử sớm việc nhìn và focus cửa sổ từ workspace ẩn mà không đổi
ownership, trước khi đầu tư nhiều vào gesture/snap.

PoC này chỉ trả lời câu hỏi khả thi và ghi bằng chứng; không mở rộng Sprint 1 thành implementation
đầy đủ của unified world. Milestone U và bộ acceptance U-1..U-6 vẫn được giữ.

**Bằng chứng:** master plan §H, dòng Phase U và các acceptance U-1..U-6.

### 2.8. Phân biệt kế hoạch tuân thủ với kết quả thực nghiệm

Bảng Constitution Check nên được hiểu là đánh giá thiết kế, không phải kết quả chạy.
License cho phần vendored còn mở nên cần quyết định được ghi nhận trước khi vendoring.

Hash patch giống nhau chưa chứng minh môi trường build có thể tái lập. Bản ghi build cần có upstream
commit, thứ tự/hash patch, toolchain và phiên bản dependency. Phân biệt rõ tái lập đầu vào,
tái lập kết quả kiểm thử và tái lập binary từng byte; không dùng một claim thay cho cả ba.

## 3. Phân công cùng làm và review

**Claude giữ vai trò tích hợp chính; Codex nhận các phần có đầu ra kiểm chứng được.**
Mỗi thời điểm, một nhóm file chỉ có một bên chịu trách nhiệm sửa.

| Gói việc | Bên làm chính | Đầu ra và điều kiện bàn giao |
|---|---|---|
| A — Build headless và provenance | Codex | Configure/build/test core và runtime khi thiếu SDL/Hyprland/Wayland; ghi commit, compiler, dependency và patch hash. Claude kiểm tra cấu hình native vẫn dùng được. |
| B — CameraMath và unit tests | Codex | Helper theo contract đã thống nhất; giữ API Camera V1; test identity, round-trip, anchor, giới hạn zoom, NaN/Inf, monitor origin khác 0. Claude kiểm tra cách dùng tại các seam compositor. |
| C — Harness và bộ kiểm chứng | Codex | Củng cố harness có sẵn; test từ chối PID/socket/signature sai; testclient, virtual pointer, analyzer và artifact kết quả. Không viết lại một harness song song. |
| D — Patch render/input | Claude | Patch series trên upstream pin, dùng helper chung; clip, culling, damage, hit-test và popup theo các cổng đã chốt. Codex review và chạy bộ kiểm chứng độc lập. |
| E — Quyết định hook hay patched build | Claude, Codex review | Hai phương án chạy cùng bộ acceptance tests; quyết định dựa trên bằng chứng, độ ổn định và chi phí bảo trì. |

### Thứ tự đề xuất sau khi được duyệt triển khai

1. Thống nhất các điểm ở mục 2; cập nhật tài liệu có thẩm quyền nếu phương án được chấp nhận.
2. Codex làm A trước để tạo đường build/test đáng tin cậy.
3. Chốt contract; Codex làm B, Claude chuẩn bị các điểm tích hợp.
4. Hoàn thiện C đủ để tạo baseline stock trước khi đánh giá patch D.
5. Review chéo từng nhóm patch; sau render/input proof mới chạy PoC unified world và delivery spike.

Mỗi lần bàn giao gồm: commit, phạm vi thay đổi, lệnh tái lập, kết quả test, artifact và phần chưa kiểm tra.
Claude tích hợp tuần tự. Tránh hai agent cùng sửa CMake hoặc cùng sửa một patch; thay đổi interface dùng
chung phải được thống nhất trước khi bên còn lại sử dụng.

Cloud dùng cho công việc không phụ thuộc compositor thật. Các kiểm tra cần GPU/Wayland chạy trên môi trường
nested đã xác minh; kiểm tra trackpad cần phần cứng và người quan sát. Thiếu môi trường không được đổi
thành kết quả PASS.

## 4. Model đề xuất

| Công việc | Model và effort đề xuất |
|---|---|
| Claude làm kiến trúc, render/input và phân tích lỗi compositor | **Claude Opus 5.5 — high** |
| Codex làm build, CameraMath, harness và tests hằng ngày | **GPT-6.1-Sol — high** |
| Codex review cổng quan trọng hoặc tìm nguyên nhân lỗi khó | **GPT-6-Astra — xhigh**, dùng theo từng đợt |
| Claude làm tài liệu hoặc thay đổi đơn giản đã có contract rõ | **Claude Sonnet 5.5 — medium** |

**Cặp mặc định đề xuất: Opus 5.5 + GPT-6.1-Sol.** Dùng Astra khi có lỗi hoặc quyết định khó cần một lượt
kiểm tra sâu; chưa có lý do dùng mức cao nhất cho mọi việc. Đánh giá model bằng chất lượng patch,
khả năng tái lập lỗi và bằng chứng kiểm thử trên dự án; không xem tên model là bảo đảm correctness.

Tên GPT và các mức effort được xác nhận qua catalog `codex debug models` trên máy trong phiên review.
Tên Claude và mức effort được đối chiếu ngày 2026-10-10 với:
- [Models overview — Anthropic](https://platform.claude.com/docs/en/models/overview).
- [Model configuration — Claude Code](https://code.claude.com/docs/en/model-config).

Đây là phân công đề xuất của Codex, chưa phải kết quả benchmark trên HyprVX.
Quyền truy cập từng model trong tài khoản Claude Code Cloud chưa được xác minh.
Tài liệu này không thay đổi model mặc định, authentication hoặc provider settings.

## 5. Kiểm chứng, tiêu chí bàn giao và giới hạn

- **Headless:** build sạch, chạy tests hiện hữu áp dụng được, kiểm tra các cấu hình optional mà không cần compositor.
- **Toán học:** ít nhất 10⁴ mẫu có seed lưu lại; thêm kết quả kỳ vọng tính độc lập, không chỉ dùng
  forward/inverse để tự xác nhận lẫn nhau. Oracle độc lập trong tests không trở thành một implementation
  camera thứ hai trong runtime.
- **Native:** identity ×3; zoom/pan ở scale 1 và 1.25; camera động và client damage khi camera tĩnh;
  pointer ít nhất 20 điểm/trạng thái; panic 10/10; layer và popup.
- **An toàn harness:** sai identity phải bị từ chối trước khi gửi lệnh; stale session không được dẫn đến
  signal nhầm tiến trình. Tận dụng kiểm tra PID/start-time/socket hiện có, bổ sung test và phần thiếu.
- **Nghiệm thu:** Claude review phần Codex viết và ngược lại. Thiếu môi trường hoặc artifact được ghi
  `BLOCKED`/`NOT RUN`; cloud không thay thế xác nhận GPU và trackpad thật.

Giả định: tiếp tục kiến trúc hybrid, giữ Claude tích hợp chính, ưu tiên chất lượng ở phần compositor và
tiết kiệm usage ở việc thông thường. Các gói việc trên chỉ bắt đầu khi người dùng duyệt phạm vi triển khai
theo quy trình dự án; việc ghi tài liệu này không làm thay đổi trạng thái approval của Feature 001.

**Đã kiểm tra trong phiên review:** tài liệu, một số đường code/CMake liên quan, catalog model và phép kiểm tra
số học nhỏ về identity. Graph coverage không báo khoảng thiếu ở các file bằng chứng được chọn; đây vẫn là
kiểm tra có giới hạn, không chứng minh toàn bộ codebase đã được audit.

**Chưa thực hiện:** build, chạy test suite, chạy compositor, kiểm chứng pixel/input/trackpad hoặc benchmark model.
Tính đúng runtime của kiến trúc và các bản vá vẫn cần bằng chứng thực nghiệm.
