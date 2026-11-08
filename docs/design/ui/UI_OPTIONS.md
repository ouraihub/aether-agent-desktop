# C17桌面界面候选评估

日期：2026-11-08。用户要求重新评估GTK以外的UI；本文件是资料评估，不引入依赖、不把原生运行/IME/无障碍写成已通过。GTK仍是DEC-09候选，不以已有烟测锁死选择。

## 判断标准

Aether核心/业务维持ISO C17，UI边界可以隔离第三方C++/Objective-C实现。必须区分公开C API、库内部语言、界面描述语言三件事：WebView的C宿主不等于HTML/JS是C；cimgui的C入口不等于Dear ImGui纯C；原生API的封装不等于统一样式可随意控制。

目标不是简单表单，而是用户给定Autorail工作台与模型服务页面：多栏、树/标签/弹层、长消息/代码/工具块、可调整面板、主题token、中文组合输入、键盘/焦点/辅助功能和安全Backend交接。

## 比较

| 方案 | C边界/平台 | 主题Token与Autorail还原 | 关键风险与结论 |
| --- | --- | --- | --- |
| GTK4 | 正式C API；Windows/macOS/Linux有后端；LGPL-2.1-or-later | 命名颜色/CSS/组件类可统一主题，布局和自定义控件可实现；本轮同源token实际GTK切换通过 | 原生Windows/macOS打包和三平台IME/AT仍未验；是坚持C控件路线的主候选，不是因现有烟测自动获选 |
| NAppGUI | ANSI-C API，主体C、少量C++/Objective-C；Win32/AppKit/GTK后端；MIT | Layout/SplitView/View可组合工作台，原生控件为主；可做应用层token映射，但不是CSS变量换皮系统 | 原生控件跨平台完整换色与自绘控件AT成本；尚缺本项目可核查的三平台accessibility实测。适合偏原生外观/小分发，列为首个非GTK原生候选 |
| libui-ng | C API，原生平台后端，Unix依赖GTK3；MIT | 基础表单、表格、Area绘图；无与网页等价的完整主题系统 | 官方明确mid-alpha；复杂工作台/一致token需补太多控件与主题层。不推荐主产品 |
| IUP | C/Lua/LED API，主要原生控件 | 属性式样式可映射token，成熟基础控件 | macOS原生后端及当前可分发版本/三平台IME/AT需重新核实；不能因为称multiplatform就当原生macOS已满足。不列首选 |
| LVGL | C核心、MIT，跨平台需display/input/窗口驱动，偏嵌入式 | styles/theme/flex/grid适合自绘一致外观，token映射自然 | UTF-8/CJK渲染不等于桌面IME composition；系统无障碍、桌面文本编辑/选中/长历史需要验证和适配。可画得像，但桌面能力不是免费获得。不选当前主线 |
| Nuklear（或SDL+自绘） | ANSI C单头；窗口/输入/渲染后端另配；Nuklear按其许可选择 | 高度可定制，应用层style可用token | 缺完整桌面文本/IME/AT/长消息控件，容易变成维护自研GUI。适合内部工具，不推荐本产品 |
| Clay | C布局引擎，zlib；窗口/渲染/文本编辑由宿主提供 | Flexbox式布局、arena内存和自定义绘制适合一致token | 不是完整控件工具包，不能把布局演示当作IME/编辑器/无障碍成熟。适合愿意投入自绘组件的项目，不是本产品快速落地路线 |
| EFL / Elementary | C实现及C API，Evas/Edje/Ecore/Elementary体系 | 完整控件、场景图、Edje主题适合定制；比纯布局库完整 | 库族与主题工具链复杂，非Linux桌面打包和IME/AT需逐平台验证；可作为定制C桌面候选，不因Linux成熟推断三平台成熟 |
| Agar / LibAgar | C工具包、BSD许可，官方列X11/Windows/macOS/SDL后端 | 标准控件和style engine，适合工程/可视化工具 | 产品生态与现代桌面IME/AT/打包需验证；值得了解，但当前无本项目实测支持其优于GTK |
| raygui | C即时模式，zlib，通常配raylib | 25+控件和运行时style，适合编辑器/小工具 | 官方定位tools development；长文本/复杂消息/IME/无障碍仍需补足，不当完整桌面应用框架 |
| webview + HTML/CSS | C公开API，库内部C++；Windows WebView2、macOS WebKit、Linux WebKitGTK；wrapper MIT，平台运行时另核查 | CSS变量可直接承载token，三栏设置和消息/代码布局最容易精确对齐Autorail；当前HTML预览可成为视觉输入 | 不是纯C渲染；新增JS/DOM前端边界、C桥接/CSP/导航限制、运行时与跨引擎验收成本。若允许独立Web前端，这是最值得与GTK对比的方案；并不等于Electron，Linux也不是完全摆脱GTK |
| Sciter | C API，HTML/CSS/script引擎，三平台发行由供应商提供 | 易做统一外观/主题，和WebView思路相近 | 专有引擎/许可分发条件和版本限制需核查，不按开源C库处理；当前不优先 |
| Qt/FLTK/wxWidgets/Dear ImGui C包装 | 主体C++，部分有非官方/生成C包装 | Qt/QML和ImGui各有主题能力 | 不能把非官方C包装等价成熟稳定C API；Qt/QML还增加UI语言。未因追求“C入口”引入另一整套体系 |

## 结论与下一步

**当前用户决定：排除WebView；NAppGUI作为当前Frontend实现入口。** GTK/EFL仅曾用于比较，实验代码已清理；未来替换只允许发生在独立`ui_adapter`边界，不改变Core/Backend契约。

NAppGUI当前使用v1.6.3，Windows原生后端与Linux GTK3后端已构建/运行；正式产品仍需Backend Client、UI Queue、中文IME、焦点、DPI、AT、macOS和关闭链验收。NAppGUI不提供Autorail同款开关，当前启用控件为原生Check Button，后续可在NAppGUI Frontend内部替换为自定义控件而不越过Backend边界。

## 一手资料

- [GTK CSS](https://docs.gtk.org/gtk4/css-overview.html)、[GTK accessibility](https://docs.gtk.org/gtk4/section-accessibility.html)。
- [NAppGUI定位与实现语言](https://www.nappgui.com/en/start/start.html)、[GUI/原生后端/HiDPI](https://www.nappgui.com/en/gui/gui.html)。
- [libui-ng官方状态与平台](https://github.com/libui-ng/libui-ng)、[IUP官方Product](https://www.tecgraf.puc-rio.br/iup/en/prod.html)。
- [LVGL官方能力与许可](https://github.com/lvgl/lvgl)、[Nuklear官方范围](https://github.com/Immediate-Mode-UI/Nuklear)。
- [webview官方C示例/平台/依赖](https://github.com/webview/webview)：C示例仍需要C++编译器构建库；不是纯C库。
- [Sciter官方C API及平台](https://sciter.com/)、[许可入口](https://sciter.com/prices/)。
- [Clay：明确定位为布局库](https://github.com/nicbarker/clay)、[EFL：C API及库族](https://www.enlightenment.org/about-efl)、[Agar：控件、样式和平台](https://libagar.org/)、[raygui：工具开发与样式](https://github.com/raysan5/raygui)。
- [cimgui](https://github.com/cimgui/cimgui)为Dear ImGui生成C API，底层C++、MIT；适合开发工具，不能归类为纯C完整桌面套件。Sciter仅SDK部分开放/二进制许可，不列作开源C引擎推荐。
