#!/usr/bin/env python3
"""
Generate QmClient multi-language translation files under data/qmclient/languages/.

Strategy:
- Each language gets a file with the same base name as DDNet's data/languages/<lang>.txt
- Source code uses English keys, matching DDNet's localization model
- simplified_chinese.txt: English keys → Simplified Chinese translations
- english.txt is generated empty because English falls back to the source key
- Other languages: English keys → English placeholder until translated by the community
  They should later be translated by the community.

Format: Same as DDNet — key on one line, == translation on next line.
"""

import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
STRINGS_FILE = os.path.join(SCRIPT_DIR, "extracted_strings.txt")
OUTPUT_DIR = os.path.join(PROJECT_ROOT, "data", "qmclient", "languages")
BASE_SIMPLIFIED_CHINESE = os.path.join(
    PROJECT_ROOT, "data", "languages", "simplified_chinese.txt"
)
BASE_LANGUAGES_DIR = os.path.join(PROJECT_ROOT, "data", "languages")

# ---------------------------------------------------------------------------
# Language metadata — MUST match DDNet's data/languages/index.txt
# We add "english" which DDNet hardcodes but doesn't have a file for.
# ---------------------------------------------------------------------------
LANGUAGES = [
    ("arabic", "العربية", "682", "ar"),
    ("azerbaijani", "Azərbaycan dili", "031", "az"),
    ("belarusian", "Беларуская", "112", "be"),
    ("bosnian", "Bosanski", "070", "bs-Latn"),
    ("brazilian_portuguese", "Português brasileiro", "076", "pt-BR"),
    ("bulgarian", "Български", "100", "bg"),
    ("catalan", "Català", "906", "ca"),
    ("chuvash", "Чăвашла", "643", "cv"),
    ("czech", "Česky", "203", "cs"),
    ("danish", "Dansk", "208", "da"),
    ("dutch", "Nederlands", "528", "nl"),
    ("english", "English", "826", "en"),
    ("esperanto", "Esperanto", "999", "eo"),
    ("estonian", "Eesti", "233", "et"),
    ("finnish", "Suomi", "246", "fi"),
    ("french", "Français", "250", "fr"),
    ("galician", "Galego", "906", "gl"),
    ("german", "Deutsch", "276", "de"),
    ("greek", "Ελληνικά", "300", "el"),
    ("hungarian", "Magyar", "348", "hu"),
    ("italian", "Italiano", "380", "it"),
    ("japanese", "日本語", "392", "ja"),
    ("korean", "한국어", "410", "ko"),
    ("kyrgyz", "Кыргызча", "417", "ky"),
    ("norwegian", "Norsk", "578", "no"),
    ("persian", "فارسی", "364", "fa"),
    ("polish", "Polski", "616", "pl"),
    ("portuguese", "Português", "620", "pt"),
    ("romanian", "Română", "642", "ro"),
    ("russian", "Русский", "643", "ru"),
    ("serbian", "Српски (Latinica)", "688", "sr-Latn"),
    ("serbian_cyrillic", "Српски (Ћирилица)", "688", "sr-Cyrl"),
    ("simplified_chinese", "简体中文", "156", "zh-Hans;zh-CN"),
    ("slovak", "Slovenčina", "703", "sk"),
    ("spanish", "Español", "724", "es"),
    ("swedish", "Svenska", "752", "sv"),
    ("traditional_chinese", "繁體中文", "158", "zh-Hant;zh-TW;zh-HK"),
    ("turkish", "Türkçe", "792", "tr"),
    ("ukrainian", "Українська", "804", "uk"),
]


def is_chinese(s):
    """Return True if s contains any CJK characters."""
    for ch in s:
        if "\u4e00" <= ch <= "\u9fff" or "\u3400" <= ch <= "\u4dbf":
            return True
    return False


# ---------------------------------------------------------------------------
# English translations for Chinese keys (manually curated)
# ---------------------------------------------------------------------------
EN_TRANSLATIONS = {
    # ── 通用 / General ──
    "关": "Off",
    "开": "On",
    "关闭": "Off",
    "默认": "Default",
    "自动": "Auto",
    "自定义": "Custom",
    "两者": "Both",
    "字面量": "Literal",
    "正则": "Regex",
    "正则表达式": "Regular expression",
    "重置": "Reset",
    "发送": "Send",
    "刷新": "Refresh",
    "设置颜色": "Set color",
    "透明度": "Opacity",
    "选项颜色": "Option color",
    "背景颜色": "Background color",
    "文字颜色": "Text color",
    "控制台颜色": "Console color",
    "辅助线颜色": "Guide line color",
    "线宽": "Line width",
    "动画": "Animation",
    "改名": "Rename",
    "翻译": "Translate",
    "语音": "Voice",
    "麦克风": "Microphone",
    "扬声器": "Speaker",
    # ── Tab & Module names ──
    "消息气泡": "Chat Bubble",
    "Gores演员专用": "Gores Actor",
    "Gores模式": "Gores Mode",
    "禅模式": "Zen Mode",
    "按键绑定": "Key Bindings",
    "梦的小功能": "Dream Features",
    "武器辅助线": "Weapon Trajectory",
    "镜头与视野": "Camera & FOV",
    "分身小窗": "Dummy Window",
    "显示坐标": "Show Coordinates",
    "主播模式": "Streamer Mode",
    "好友提醒": "Friend Notifications",
    "屏蔽词": "Word Filter",
    "聊天翻译按钮UI": "Chat Translate Button UI",
    "关键词回复": "Keyword Reply",
    "饼菜单": "Pie Menu",
    "实体层颜色": "Entity Layer Colors",
    "激光增强": "Laser Enhancement",
    "玩家统计": "Player Stats",
    "HJ辅助": "HJ Assist",
    "速通计时器": "Speedrun Timer",
    "灵动岛": "Dynamic Island",
    "SMTC": "SMTC",
    "3D背景": "3D Background",
    "功能搜索": "Feature Search",
    # ── Descriptions ──
    "在Tee上方显示对话气泡": "Show chat bubbles above Tees",
    "在玩家头顶显示聊天消息": "Show chat messages above players",
    "在水中死亡时自动说话": "Auto chat when dying in water",
    "Gores自动武器切换": "Gores auto weapon switch",
    "隐去UI专注游戏": "Hide UI for focused gameplay",
    "常见的按键绑定": "Common key bindings",
    "常用按键绑定": "Common key bindings",
    "只有你想不到,没有梦做不到": "Only what you can't imagine, nothing Dream can't do",
    "只有你想不到，没有梦做不到！": "Only what you can't imagine, nothing Dream can't do!",
    "显示枪榴弹和激光轨迹预览": "Show grenade and laser trajectory preview",
    "调整游戏镜头和视野设置": "Adjust game camera and FOV settings",
    "看看你操控本体的时候分身有没有被别人欺负": "Check if your dummy is being bullied while you control the main tee",
    "在你操控本体的时候分身有没有被欺负呢?": "Is your dummy being bullied while you control the main tee?",
    "显示自己和他人的坐标": "Show coordinates of yourself and others",
    "他妈的炸弹人都给我死啊!": "Damn grenadiers, just die!",
    "他妈的炸弹人全给我去Spa!": "Damn grenadiers, all go to spa!",
    "好友上线和加入提醒": "Friend online and join notifications",
    "聊天屏蔽词过滤": "Chat word filtering",
    "聊天翻译设置": "Chat translation settings",
    "聊天框按钮外观设置": "Chat box button appearance settings",
    "我 是 机 器 人": "I am a robot",
    "对玩家快捷操作菜单": "Quick action menu for players",
    "调整各个实体层的透明度": "Adjust opacity of entity layers",
    "你想做出怎样的激光?": "What kind of laser do you want?",
    "看看你有多演": "See how much you act",
    "显示碰撞和武器交互": "Show collision and weapon interaction",
    "你最爱的地图管家": "Your favorite map manager",
    "事已至此,多说无益": "What's done is done, no use saying more",
    "你想跑出怎样的Gores?": "What kind of Gores do you want to run?",
    "谁把OBS塞进来了": "Who stuffed OBS in here",
    "当然是最好的语音啦!": "The best voice chat, of course!",
    "Only Apple Can Do": "Only Apple Can Do",
    "基于Win的一坨所拉的一泡": "Based on a pile of Windows... stuff",
    "配置背景 3D 粒子效果": "Configure background 3D particle effects",
    "快速定位功能模块": "Quickly locate feature modules",
    "你想拍出怎样的大片": "What kind of blockbuster do you want to shoot?",
    "你想跑出怎样的人生?": "What kind of life do you want to run?",
    "画一张大饼": "Drawing a big pie in the sky",
    "噢噢噢噢噢噢噢噢噢噢噢噢噢噢噢噢": "Ohhhhhhhhhhhhhhhh",
    "Teeworlds的世界不会再出现挡人的实体层了": "No more blocking entity layers in the world of TeeWorlds",
    "在玩家头顶显示坐标": "Show coordinates above players",
    "显示玩家头顶的消息气泡": "Show chat bubbles above players",
    "水中自动发言": "Auto chat in water",
    "水中发送表情": "Send emoticon in water",
    "留空以禁用": "Leave empty to disable",
    "留空以进入公共房间": "Leave empty to join public room",
    "留空使用默认提示词": "Leave empty to use default prompt",
    "用逗号分隔": "Separate with commas",
    "示例: 名字1|名字2|名字3": "Example: name1|name2|name3",
    "请使用 %s 作为好友名称": "Please use %s as friend name",
    "用 ID 替换非好友名称": "Replace non-friend names with ID",
    "用默认皮肤替换非好友皮肤": "Replace non-friend skins with default",
    "在记分板上使用默认旗": "Use default flags on scoreboard",
    "显示自己的坐标": "Show own coordinates",
    "显示其他玩家的坐标": "Show other players' coordinates",
    "与我 X 对齐提示": "X alignment hint with me",
    "严格模式": "Strict mode",
    "判定时间": "Detection time",
    "显示 X": "Show X",
    "显示 Y": "Show Y",
    "X 对齐颜色": "X alignment color",
    "启用 Gores 模式": "Enable Gores mode",
    "Gores 模式下自动启用": "Auto enable in Gores mode",
    "Gores 模式按键": "Gores mode key",
    "启用禅模式": "Enable Zen mode",
    "禅模式按键": "Zen mode key",
    "启用复读": "Enable repeat",
    "启用关键词回复": "Enable keyword reply",
    "启用屏蔽词列表": "Enable word filter list",
    "启用思考模式（较慢）": "Enable thinking mode (slower)",
    "启用相机漂移": "Enable camera drift",
    "启用立体声定位": "Enable stereo positioning",
    "启用系统媒体控制": "Enable system media control",
    "启用语音": "Enable voice",
    "启用饼菜单": "Enable pie menu",
    "启用 3D 背景粒子": "Enable 3D background particles",
    "启用 FTAPI 自动翻译（可能导致服务过载）": "Enable FTAPI auto-translate (may overload the service)",
    "启用分身小窗": "Enable dummy window",
    "启用动态FOV": "Enable dynamic FOV",
    "自动登录 Axiom 服务器": "Auto login Axiom server",
    "自动刷新服务器列表": "Auto refresh server list",
    "自动问候进图的好友": "Auto greet friends entering map",
    "自动检测说话时开麦": "Auto unmute when speaking",
    "自动平衡麦克风音量（AGC，实验性）": "Auto gain control for mic (AGC, experimental)",
    "当好友上线时提醒": "Notify when friends come online",
    "在控制台中显示被屏蔽的词语": "Show blocked words in console",
    "在左上角显示歌曲信息": "Show song info in top-left corner",
    "大字播报进服好友": "Large text announcement for friend joining",
    "大字文案": "Large text content",
    "招呼文案": "Greeting text",
    "本地粒子效果": "Local particle effects",
    "远程粒子效果": "Remote particle effects",
    "计分板Qm标识": "Scoreboard Qm badge",
    "计分板查分": "Scoreboard point check",
    "显示版本过旧提示": "Show outdated version warning",
    "新版UI": "New UI",
    "锤中偷皮": "Hammer skin steal",
    "随机表情": "Random emoticon",
    "连击": "Combo",
    "隐藏输入表情": "Hide input emoticon",
    "彩虹名字": "Rainbow name",
    "位置跳跃提示": "Position jump hint",
    "显示模式": "Display mode",
    "始终显示": "Always show",
    "按键显示": "Show on key",
    "显示碰撞箱模式": "Show hitbox mode",
    "显示 Tee、地图和武器交互碰撞箱": "Show Tee, map, and weapon interaction hitboxes",
    "碰撞箱模式": "Hitbox mode",
    "武器交互范围": "Weapon interaction range",
    "武器范围颜色": "Weapon range color",
    "Tee 碰撞箱": "Tee hitbox",
    "Tee 碰撞箱颜色": "Tee hitbox color",
    "Freeze 边界颜色": "Freeze border color",
    "拾取物范围": "Pickup range",
    "拾取其他武器后禁用": "Disable after picking up other weapons",
    "隐藏辅助线": "Hide guide lines",
    "发送概率": "Send probability",
    "聊天消息": "Chat message",
    "表情 ID": "Emoticon ID",
    "关键词规则": "Keyword rules",
    "关键词规则的两侧都必须填写": "Both sides of keyword rules must be filled",
    "替换字符": "Replacement chars",
    "替换模式": "Replace mode",
    "根据词语长度使用多字符替换": "Use multi-char replacement based on word length",
    "最少匹配字符数": "Minimum match chars",
    "系统媒体控制": "System media control",
    "动态FOV平滑度": "Dynamic FOV smoothness",
    "动态FOV强度": "Dynamic FOV intensity",
    "漂移平滑度": "Drift smoothness",
    "漂移强度": "Drift intensity",
    "漂移方向": "Drift direction",
    "当前宽高比:": "Current aspect ratio:",
    "当前宽高比: 显示默认": "Current aspect ratio: Show default",
    "自定义宽高比": "Custom aspect ratio",
    "宽高比预设": "Aspect ratio preset",
    "分身小窗大小": "Dummy window size",
    "分身小窗缩放": "Dummy window zoom",
    "仅本地": "Local only",
    "本地 + 分身": "Local + Dummy",
    "所有玩家": "All players",
    "显示队伍": "Show team",
    "玩家范围": "Player range",
    "检测距离": "Detection distance",
    "地图危险边界": "Map danger border",
    "解冻不透明度": "Unfreeze opacity",
    "冻结不透明度": "Freeze opacity",
    "深度冻结不透明度": "Deep freeze opacity",
    "深度解冻不透明度": "Deep unfreeze opacity",
    "死亡不透明度": "Death opacity",
    "CP点不透明度": "CP opacity",
    "传送不透明度": "Teleport opacity",
    "开关不透明度": "Switch opacity",
    "叠层不透明度": "Tune layer opacity",
    "进度条颜色": "Progress bar color",
    "进度条宽度": "Progress bar width",
    "进度条高度": "Progress bar height",
    "饼菜单透明度": "Pie menu opacity",
    "翻译按钮": "Translate button",
    "翻译服务": "Translation service",
    "LLM 提供商": "LLM provider",
    "目标语言": "Target language",
    "目标语言比例": "Target language ratio",
    "并发数（0 = 自动）": "Concurrency (0 = auto)",
    "发送源语言": "Send source language",
    "发送目标语言": "Send target language",
    "发送翻译方式": "Send translation method",
    "始终翻译": "Always translate",
    "仅在需要时翻译": "Translate only when needed",
    "自动翻译发送的消息": "Auto translate sent messages",
    "自动翻译收到的消息": "Auto translate received messages",
    "翻译按钮和菜单的颜色": "Translate button and menu colors",
    "自定义翻译按钮和菜单的颜色": "Customize translate button and menu colors",
    "高级选项": "Advanced options",
    "语音激活释放延迟": "Voice activation release delay",
    "语音码率": "Voice bitrate",
    "语音距离半径（格）": "Voice distance radius (tiles)",
    "说话触发阈值": "Speech trigger threshold",
    "麦克风音量": "Microphone volume",
    "静音麦克风": "Mute microphone",
    "播放音量": "Playback volume",
    "输入设备": "Input device",
    "输出设备": "Output device",
    "输入切换": "Input switch",
    "输出切换": "Output switch",
    "降噪模式": "Noise reduction mode",
    "不降噪": "No noise reduction",
    "简单降噪": "Simple noise reduction",
    "简单降噪强度": "Simple noise reduction strength",
    "RNNoise 降噪": "RNNoise noise reduction",
    "RNNoise 降噪强度": "RNNoise noise reduction strength",
    "RNNoise 降噪（当前构建不可用）": "RNNoise noise reduction (unavailable in this build)",
    "当前构建未集成 RNNoise，将自动回退到简单降噪": "RNNoise not integrated in current build, will fallback to simple noise reduction",
    "回退后的简单降噪强度": "Fallback simple noise reduction strength",
    "系统默认": "System default",
    "立体声定位": "Stereo positioning",
    "左右声道宽度": "Left/right channel width",
    "房间": "Room",
    "房间密码": "Room password",
    "同房间全图收听": "Full map listen in same room",
    "服务器IP": "Server IP",
    "等待打开": "Waiting to open",
    "已打开": "Opened",
    "已连接": "Connected",
    "已断开": "Disconnected",
    "已静音": "Muted",
    "已切换": "Switched to",
    "切换失败": "Switch failed",
    "未启用": "Not enabled",
    "语音连接状态": "Voice connection status",
    "音频问题": "Audio issue",
    "音频异常未分类": "Unclassified audio issue",
    "音频后端初始化失败": "Audio backend init failed",
    "音频后端初始化失败，建议切换设备后重试，并查看详细原因": "Audio backend init failed, try switching devices and check details",
    "音频初始化失败，建议查看下方详细原因并结合日志排查": "Audio init failed, check details below and logs",
    "系统没有可用输入设备": "No input device available",
    "系统没有可用输出设备": "No output device available",
    "输入设备不存在": "Input device not found",
    "输出设备不存在": "Output device not found",
    "输入设备打开失败": "Input device open failed",
    "输出设备打开失败": "Output device open failed",
    "输入设备打开失败，建议重插耳机/麦克风后刷新，或重新选择输入设备": "Input device open failed, try reconnecting headset/mic or reselecting input device",
    "输出设备打开失败，建议重插耳机/扬声器后刷新，或重新选择输出设备": "Output device open failed, try reconnecting speakers/headphones or reselecting output device",
    "默认输入打开失败": "Default input open failed",
    "默认输出打开失败": "Default output open failed",
    "正在切回默认输入": "Switching back to default input",
    "正在切回默认输出": "Switching back to default output",
    "正在切换": "Switching",
    "等待打开输入设备": "Waiting to open input device",
    "等待打开输出设备": "Waiting to open output device",
    "未打开，请检查输入设备或麦克风权限": "Not open, check input device or mic permission",
    "未打开，请检查输出设备": "Not open, check output device",
    "麦克风权限被系统拒绝": "Microphone permission denied by system",
    "需要在系统设置里允许麦克风权限，然后重新打开语音": "Allow mic permission in system settings, then reopen voice",
    "建议先检查输入设备、系统默认麦克风和麦克风权限": "Check input device, system default mic, and mic permission first",
    "建议先检查输出设备，确认耳机或扬声器仍在线": "Check output device, confirm headphones/speakers are still online",
    "建议重新选择输入设备，并确认默认麦克风或耳机麦克风仍在线": "Try reselecting input device, confirm default mic or headset mic is online",
    "建议重新选择输出设备，并确认耳机或扬声器仍在线": "Try reselecting output device, confirm headphones/speakers are online",
    "建议重新切换语音开关或重试进入服务器": "Try toggling voice or reconnecting to server",
    "建议检查语音服务器地址是否可用": "Check if voice server address is reachable",
    "建议确认双方在同服、同房间，并且对方也支持语音": "Confirm both are on same server, same room, and support voice",
    "未进入服务器": "Not connected to server",
    "需要先进入服务器，语音网络链路才会建立": "Connect to server first to establish voice network link",
    "未发现音频异常": "No audio issues detected",
    "请先启用语音": "Please enable voice first",
    "本地测试模式": "Local test mode",
    "本地测试模式，无需服务器": "Local test mode, no server needed",
    "本地正在发送，建议让对方开麦或确认对方是否能接收": "Sending locally, suggest the other party unmute or confirm they can receive",
    "正在发送，等待对端回声": "Sending, waiting for peer echo",
    "正在发送和接收": "Sending and receiving",
    "正在接收": "Receiving",
    "已匹配到可通话对端": "Matched with callable peer",
    "当前未发现可通话对端": "No callable peer found",
    "暂无对端": "No peer",
    "未知状态": "Unknown status",
    "当前状态正常，如仍异常请查看下方详细原因": "Status normal, check details below if still experiencing issues",
    "详细原因": "Detailed reason",
    "建议排查": "Troubleshooting suggestions",
    "正在解析语音服务器地址": "Parsing voice server address",
    "已连接，等待首个 ping": "Connected, waiting for first ping",
    "已联通，暂时无人说话": "Connected, no one is speaking",
    "更新功能列表...": "Updating feature list...",
    "未找到匹配的功能模块。请尝试其他关键词": "No matching features found. Try other keywords",
    "匹配到 %d 个模块": "Found %d modules",
    "手动并发数：%d": "Manual concurrency: %d",
    "自动并发数：%d（智能默认）": "Auto concurrency: %d (smart default)",
    "使用默认输入": "Use default input",
    "使用默认输出": "Use default output",
    "使用原始样式": "Use original style",
    "使用分身回复": "Reply with dummy",
    "显示语音连接状态": "Show voice connection status",
    "先看这里，可以快速判断卡在设备、服务器还是房间": "Start here to quickly diagnose if the issue is with device, server, or room",
    "UDP 套接字未打开": "UDP socket not open",
    "仅当另一个Tee不在屏幕上时显示": "Only show when the other Tee is not on screen",
    "异常断开": "Abnormal disconnect",
    "傻逼词过滤器": "Idiot word filter",
    "上一首": "Previous",
    "下一首": "Next",
    "播放/暂停": "Play/Pause",
    "显示歌曲信息": "Show song info",
    "系统媒体控制 (SMTC)": "System Media Transport Controls (SMTC)",
    "改名队列": "Rename queue",
    "自定义提示词模板": "Custom prompt template",
    "自定义 API Key": "Custom API Key",
    "API Key": "API Key",
    "API key": "API key",
    "SecretId": "SecretId",
    "SecretKey": "SecretKey",
    "端点": "Endpoint",
    "端点 (可选)": "Endpoint (optional)",
    "模型": "Model",
    "区域": "Region",
    "Append language codes like [ru], [en], [ja] at the end when sending": "Append language codes like [ru], [en], [ja] at the end when sending",
    "Enable thinking mode requires using reasoning models": "Enable thinking mode requires using reasoning models",
    "Ensure backend supports OpenAI-compatible thinking parameter": "Ensure backend supports OpenAI-compatible thinking parameter",
    "空": "Empty",
    "已选中": "Selected",
    "所有": "All",
    "取消": "Cancel",
    "确定": "Confirm",
    "保存": "Save",
    "删除": "Delete",
    "添加": "Add",
    "编辑": "Edit",
    "搜索": "Search",
    "加载中": "Loading",
    "暂无数据": "No data",
    "复制": "Copy",
    "复制成功": "Copied",
    "复制失败": "Copy failed",
    "清空": "Clear",
    "启用": "Enable",
    "禁用": "Disable",
    "成功": "Success",
    "失败": "Failed",
    "警告": "Warning",
    "错误": "Error",
    "信息": "Info",
    "确认": "Confirm",
    "是": "Yes",
    "否": "No",
    "提示": "Hint",
    "详情": "Details",
    "更多": "More",
    "收起": "Collapse",
    "展开": "Expand",
    "上一页": "Previous page",
    "下一页": "Next page",
    "首页": "First page",
    "尾页": "Last page",
    "共 %d 条": "Total %d items",
    "打开颜色选择器": "Open color picker",
    "点击以设置颜色": "Click to set color",
    "大小": "Size",
    "最大尺寸": "Max size",
    "最小尺寸": "Min size",
    "背景不透明度": "Background opacity",
    "UI缩放": "UI scale",
    "气泡不透明度": "Bubble opacity",
    "粒子类型": "Particle type",
    "粒子颜色": "Particle color",
    "粒子数量": "Particle count",
    "粒子透明度": "Particle alpha",
    "粒子发光": "Particle glow",
    "粒子脉动": "Particle pulse",
    "粒子拖尾": "Particle trail",
    "粒子碰撞": "Particle collision",
    "粒子速度": "Particle speed",
    "粒子深度": "Particle depth",
    "粒子闪烁": "Particle twinkle",
    "脉动幅度": "Pulse amplitude",
    "脉动速度": "Pulse speed",
    "脉动强度": "Pulse strength",
    "闪烁强度": "Twinkle strength",
    "拖尾长度": "Trail length",
    "拖尾透明度": "Trail alpha",
    "发光强度": "Glow intensity",
    "发光透明度": "Glow alpha",
    "发光偏移": "Glow offset",
    "推动半径": "Push radius",
    "推动强度": "Push strength",
    "输入叠加层": "Input overlay",
    "输入叠加层显示": "Input overlay display",
    "输入叠加层不透明度": "Input overlay opacity",
    "显示输入": "Show inputs",
    "水平位置": "Horizontal position",
    "垂直位置": "Vertical position",
    "视野边距": "View margin",
    "动态FOV": "Dynamic FOV",
    "相机漂移": "Camera drift",
    "激光预览": "Laser preview",
    "激光设置": "Laser settings",
    "激光大小": "Laser size",
    "激光样式": "Laser style",
    "圆角端点": "Rounded caps",
    "自动禁用时间": "Auto disable when time expires",
    "暂停后自动关闭当前聊天": "Automatically close the current chat after waking from freeze",
    "显示冰冻后的唤醒弹窗": "Show wake-up popup on the other tee",
    "自动换到刚解冻的Tee": "Auto switch to the tee that got unfrozen",
    "自动解除旁观": "Auto unspec on unfreeze",
    "自动锁定队伍": "Auto team lock",
    "自动热重载外部保存": "Auto hot-reload after external saves",
    "不解冻时自动锁定": "Lock delay",
    "地图进度条（实验性）": "Map progress bar (experimental)",
    "使用嵌入式HUD进度条": "Use embedded HUD progress bar",
    "显示虚线地图路线调试": "Show dotted map route debug",
    "启用速通计时器": "Enable speedrun timer",
    "速通倒计时": "Countdown timer for speedruns",
    "重置统计数据": "Reset stats when joining a server",
    "显示玩家统计HUD": "Show player stats HUD",
    "收藏地图管理": "Favorite map manager",
    "收藏的地图": "Favorite maps",
    "暂无收藏地图": "No favorite maps yet",
    "从收藏移除": "Remove from favorites",
    "点击复制地图名称": "Click to copy the map name",
    "复制地图名称": "Copy map name",
    "已复制": "Copied",
    "地图收藏": "Map favorites",
    "玩家统计与信息": "Player stats and info display",
    "换肤": "Swap skin",
    "观战": "Spectate",
    "分身": "Dummy",
    "好友": "Friend",
    "私聊": "Whisper",
    "提到": "Mention",
    "事件": "Event",
    "乐趣": "Fun",
    "Solo": "Solo",
    "Race": "Race",
    "Dummy": "Dummy",
    "Unknown": "Unknown",
    # ── Additional translations ──
    "45°瞄准": "45° Aim",
    "Axiom 主号密码": "Axiom main account password",
    "Axiom 分身密码": "Axiom dummy password",
    "Gores 模式": "Gores Mode",
    "King of Gores 自动切换枪": "King of Gores auto weapon switch",
    "TeeWorlds的世界不会再出现挡人的实体层了": "No more blocking entity layers in the world of TeeWorlds",
    "⚠️ FTAPI 是一个免费服务。过度使用可能导致服务暂停。": "⚠️ FTAPI is a free service. Excessive use may cause service suspension.",
    "仅在另一个Tee不在屏幕上时显示": "Only show when the other Tee is not on screen",
    "右跳": "Right jump",
    "左跳": "Left jump",
    "已切换到": "Switched to",
    "帧率": "Frame rate",
    "弹跳": "Bounce",
    "当前状态": "Current status",
    "感谢您的陪伴与信任.正是如此,才给了我继续前行的勇气": "Thank you for your company and trust. This is what gives me the courage to keep going.",
    "收发": "Send & Receive",
    "收缩": "Shrink",
    "智谱 AI": "Zhipu AI",
    "智谱 API Key": "Zhipu API Key",
    "服务器": "Server",
    "消散": "Dissolve",
    "游戏时间边距": "Game time margin",
    "瞄缝救人": "Gap aim rescue",
}

QMCLIENT_SIMPLIFIED_STATIC_NOTIFICATION_TRANSLATIONS = {
    "0 队模式下不能开启练习模式": "Practice mode can't be enabled in team 0 mode.",
    "????????????????": "Your team was unlocked by an unlock team tile",
    "传送手雷已关闭": "Teleport grenade disabled",
    "传送手雷已开启": "Teleport grenade enabled",
    "传送枪已关闭": "Teleport gun disabled",
    "传送枪已开启": "Teleport gun enabled",
    "传送激光已关闭": "Teleport laser disabled",
    "传送激光已开启": "Teleport laser enabled",
    "你不能和自己交换位置": "Can't swap with yourself",
    "你不能把已授权玩家移到旁观": "You can't move authorized players to spectators",
    "你不能把自己移到旁观": "You can't move yourself to spectators",
    "你不能踢已授权玩家": "You can't kick authorized players",
    "你不能踢自己": "You can't kick yourself",
    "你不能这么快再次 /spec": "Can't /spec that quickly.",
    "你之前没有传送过，请先使用 /tp": "You haven't previously teleported. Use /tp before using this command.",
    "你切换队伍太快了": "You can't change teams that fast!",
    "你发送邀请太快了，请稍后再试": "Can't invite this quickly",
    "你只能把自己队伍里的成员移到旁观": "You can only move your team member to spectators",
    "你只能踢自己队伍里的成员": "You can kick only your team member",
    "你和对方都不能处于暂停状态，才能交换位置": "You and the other player must not be paused.",
    "你和对方都需要先开始地图，才能交换位置": "You and other player need to have started the map",
    "你失去了喷气背包枪": "You lost your jetpack gun",
    "你将不再接收全局聊天和服务器消息": "You will not receive any further global chat and server messages",
    "你将不再接收悄悄话": "You will not receive any further whispers",
    "你将继续接收全局聊天和服务器消息": "You will receive global chat and server messages",
    "你将继续接收悄悄话": "You will receive whispers",
    "你已经在这个队伍里了": "You are in this team already",
    "你已经开始比赛了": "You have started racing already",
    "你已经死亡，但会继续保持练习模式，直到你输入 kill": "You died, but will stay in practice until you use kill.",
    "你已经用过练习模式了": "You have used practice mode already",
    "你当前不在开启了 /practice 的队伍中。注意：练习模式下无法获得排名": "You're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.",
    "你当前可以看到其他玩家": "You can see other players. To disable this use DDNet client and type /showothers",
    "你当前客户端不支持所选计时器类型": "Selected timertype is not supported by your client",
    "你当前没有可用的眼部表情，记得先绑定": "You don't have any eye emotes, remember to bind some.",
    "你当前没有待处理的交换请求": "You do not have a pending swap request.",
    "你当前看起来正在使用 VPN，暂时不能发起投票。如有误判，请关闭 VPN 或联系管理员": "You are not allowed to vote because you appear to be using a VPN. Try connecting without a VPN or contacting an admin if you think this is a mistake.",
    "你必须加入一个队伍并和其他人一起玩，否则无法开始": "You must join a team and play with somebody or else you can't play",
    "你必须在队伍中（1 到 63 队）": "You have to be in a team (from 1-63)",
    "你正在发起投票，请等当前投票结束后再试": "You are running a vote, please try again after the vote is done!",
    "你现在不会再看到本服所有 tee 了": "You will no longer see all tees on this server",
    "你现在不能与其他玩家碰撞": "You can't collide with others",
    "你现在不能攻击其他玩家": "You can't hit others",
    "你现在不能用手雷攻击其他玩家": "You can't shoot others with grenade",
    "你现在不能用散弹枪攻击其他玩家": "You can't shoot others with shotgun",
    "你现在不能用激光攻击其他玩家": "You can't shoot others with laser",
    "你现在不能用锤子攻击其他玩家": "You can't hammer hit others",
    "你现在不能钩中其他玩家": "You can't hook others",
    "你现在会看到本服所有 tee，不再受距离限制": "You will now see all tees on this server, no matter the distance",
    "你现在可以与其他玩家碰撞": "You can collide with others",
    "你现在可以使用预设眼部表情": "You can now use the preset eye emotes.",
    "你现在可以攻击其他玩家": "You can hit others",
    "你现在可以用手雷攻击其他玩家": "You can shoot others with grenade",
    "你现在可以用散弹枪攻击其他玩家": "You can shoot others with shotgun",
    "你现在可以用激光攻击其他玩家": "You can shoot others with laser",
    "你现在可以用锤子攻击其他玩家": "You can hammer hit others",
    "你现在可以钩中其他玩家": "You can hook others",
    "你现在拥有喷气背包枪": "You have a jetpack gun",
    "你现在拥有无限空跳": "You have unlimited air jumps",
    "你现在没有无限空跳了": "You don't have unlimited air jumps",
    "你的 timeout code 已设置": "Your timeout code has been set. 0.7 clients can not reclaim their tees on timeout; however, a 0.6 client can claim your tee",
    "你的队伍因包含无效 tee 状态而被处死": "Your team has been killed because it contains an invalid tee state",
    "你的队伍因已无法完赛且未进入 /practice 模式而被处死": "Your team was killed because it couldn't finish anymore and hasn't entered /practice mode",
    "你的队伍已开启练习模式，祝你练习愉快！": "Practice mode enabled for your team, happy practicing!",
    "你的队伍当前正在保存": "Your team is currently saving",
    "你的队伍还没有开始": "Your team has not started yet",
    "你需要先开始地图，才能和其他玩家交换位置": "Need to have started the map to swap with a player.",
    "先加入队伍后才能使用交换功能，也就是和队友互换位置": "Join a team to use swap feature, which means you can swap positions with each other.",
    "先加入队伍才能开启练习模式。开启后可以使用 /r，但不会获得排名": "Join a team to enable practice mode, which means you can use /r, but can't earn a rank.",
    "处于 0 队模式时不能保存队伍存档": "Team can't be saved while in team 0 mode",
    "处于 0 队模式时不能读档": "Team can't be loaded while in team 0 mode",
    "如果想保留钩子状态，请在读档前先按住钩子": "Start holding the hook before loading the savegame to keep the hook",
    "已为你关闭主动管理员模式": "Active moderator mode disabled for you.",
    "已为你开启主动管理员模式": "Active moderator mode enabled for you.",
    "已启用防自杀保护。如果你确实想进入旁观，请先输入 /kill": "Kill Protection enabled. If you really want to join the spectators, first type /kill",
    "已经没有空队伍了": "No empty team left.",
    "开启练习模式时不能启用 team 0 模式": "Can't enable team 0 mode with practice mode on.",
    "开启练习模式时不能读档": "Team can't be loaded while practice is enabled",
    "强制 solo 服务器上不能使用交换功能": "Swap is not available on forced solo servers.",
    "强制暂停计时已结束，你现在可以用 /spec 退出": "The force pause timer is now over, you can exit with /spec",
    "当前不允许切换队伍模式": "Team mode change disabled",
    "当前使用 VPN 的玩家不允许发言": "Players are not allowed to chat from VPNs at this time",
    "当前正在检查你的 VPN 状态，约 30 秒后再尝试发起投票": "You are not allowed to vote because we're currently checking for VPNs. Try again in ~30 seconds.",
    "当前这个队伍不能邀请玩家": "Can't invite players to this team",
    "找不到你的队伍": "Could not find your Team",
    "无效的 X 坐标": "Invalid X coordinate.",
    "无效的 Y 坐标": "Invalid Y coordinate.",
    "无效的投票选项": "Invalid option",
    "无效的旁观目标 ID": "Invalid spectator id used",
    "无法识别 /tpxy 参数": "Can't recognize specified arguments. Usage: /tpxy x y, e.g. /tpxy 9 3.",
    "无限钩已关闭": "Endless hook has been deactivated",
    "无限钩已开启": "Endless hook has been activated",
    "有拖拽器生效时不能保存队伍存档": "Team can't be saved while a dragger is active",
    "服务器的踢人/旁观投票已不再由主动管理员模式接管": "Server kick/spec votes are no longer actively moderated.",
    "服务器的踢人/旁观投票现在会被主动管理员模式接管": "Server kick/spec votes will now be actively moderated.",
    "未找到该玩家": "Player not found",
    "未知 rescue 模式参数": "Unknown argument. Check '/rescuemode list'",
    "未知参数。可用值：default、gametimer、broadcast、both、none": "Unknown parameter. Accepted values: default, gametimer, broadcast, both, none",
    "未知表情命令": "Unknown emote... Say /emote",
    "本服务器不允许发起移至旁观投票": "Server does not allow voting to move players to spectators",
    "本服务器不允许发起踢人投票": "Server does not allow voting to kick players",
    "本服务器不允许查看 checkpoint 时间": "Showing the checkpoint times is not allowed on this server.",
    "本服务器不允许查看全局积分排行榜": "Showing the global top points is not allowed on this server.",
    "本服务器不允许查看其他玩家的全局积分": "Showing the global points of other players is not allowed on this server.",
    "本服务器不允许查看其他玩家的成绩": "Showing the times of others is not allowed on this server.",
    "本服务器不允许查看其他玩家的排名": "Showing the rank of other players is not allowed on this server.",
    "本服务器不允许查看其他玩家的队伍排名": "Showing the team rank of other players is not allowed on this server.",
    "本服务器不允许查看排行榜": "Showing the top is not allowed on this server.",
    "本服务器不允许查看队伍前 5 名": "Showing the team top 5 is not allowed on this server.",
    "本服务器不允许玩家互钩": "Players can't hook each other on this server",
    "本服务器不允许玩家碰撞": "Players can't collide on this server",
    "本服务器已禁用 /map": "/map is disabled",
    "本服务器已禁用交换功能": "Swap is disabled on this server.",
    "本服务器已禁用存档功能": "Save-function is disabled on this server",
    "本服务器已禁用显示其他队伍玩家": "Showing players from other teams is disabled",
    "本服务器已禁用练习模式": "Practice mode is disabled",
    "本服务器已禁用表情功能": "Emotes are disabled.",
    "本服务器已禁用队伍模式切换": "Team mode change is disabled on this server.",
    "本服务器已禁用队伍邀请": "Invites are disabled",
    "本服务器不允许组队；队伍上锁后，队内任意玩家死亡都会导致全队死亡": "Teams are not available on this server; if the team is locked, any team member dying will kill the whole team",
    "本服务器的成绩是公开的": "Scores are public on this server",
    "本服务器的成绩是私密的": "Scores are private on this server",
    "本服务器允许玩家互钩": "Players can hook each other on this server",
    "本服务器允许玩家碰撞": "Players can collide on this server",
    "本服务器允许组队；队伍上锁后，队内任意玩家死亡都会导致全队死亡": "Teams are available on this server; if the team is locked, any team member dying will kill the whole team",
    "本服未开启 rescue，且你所在队伍也没有开启 /practice。注意：练习模式下无法获得排名": "Rescue is not enabled on this server and you're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.",
    "死亡或旁观时不能切换队伍": "You can't change teams while you are dead/a spectator.",
    "死亡或旁观时不能查看当前队伍": "You can't check your team while you are dead/a spectator.",
    "比赛进行中时不能切换队伍模式": "Team mode can't be changed while racing",
    "比赛进行中时不能读档": "Team can't be loaded while racing",
    "没有 super 权限时不能加入 super 队伍": "You can't join super team if you don't have super rights",
    "没有可回退的位置": "There is nowhere to go back to.",
    "没有找到这个名字的玩家": "No player with this name found.",
    "用于移至旁观的客户端 ID 无效": "Invalid client id to move to spectators",
    "用于踢人的客户端 ID 无效": "Invalid client id to kick",
    "由于你已挂机，主动管理员模式已关闭": "Active moderator mode disabled because you are afk.",
    "登录后才可以发起投票": "You can only vote after logging in.",
    "目标玩家不在你的队伍里": "Player is on a different team",
    "练习模式下不能保存队伍存档": "Team save disabled for teams in practice mode",
    "要保存队伍，队内所有玩家都必须存活且不能处于 '/spec'": "To save all players in your team have to be alive and not in '/spec'",
    "计时器当前不显示": "Timer isn't displayed.",
    "该玩家已经被邀请过了": "Player already invited",
    "该队伍因人数超过允许上限而被解散": "This team was disbanded because there are more players than allowed in the team.",
    "请先等待当前投票结束，再发起新的投票": "Wait for current vote to end before calling a new one.",
    "请输入 /practice 或重新开始，否则整队将在 60 秒后被处死": "Enter /practice mode or restart to avoid the entire team being killed in 60 seconds",
    "这个命令在 solo 服务器上不可用": "Command is not available on solo servers",
    "这个队伍不能切换模式": "This team can't have the mode changed",
    "这个队伍不能被锁定": "This team can't be locked",
    "这个队伍已用 /lock 锁定，只有队伍成员才能用 /lock 解锁": "This team is locked using /lock. Only members of the team can unlock it using /lock.",
    "这个队伍已用 /lock 锁定，只有队伍成员才能邀请你或用 /lock 解锁": "This team is locked using /lock. Only members of the team can invite you or unlock it using /lock.",
    "这个队伍已经开始了": "This team started already",
    "这个队伍当前正在保存": "This team is currently saving",
    "这张地图上没有这个编号的 checkpoint 传送器": "There is no checkpoint teleporter with that index on the map.",
    "这张地图上没有这个编号的传送器": "There is no teleporter with that index on the map.",
    "队伍功能已禁用": "Teams are disabled",
    "队伍存档已在进行中": "Team save already in progress",
    "队伍已经处于练习模式": "Team is already in practice mode",
    "队伍正在存档或读档时，不能开启练习模式": "Practice mode can't be enabled while team save or load is in progress",
    "队伍读档已在进行中": "Team load already in progress",
    "你必须加入队伍才能在本服务器游玩；队伍上锁后，队内任意玩家死亡都会导致全队死亡": "You have to be in a team to play on this server and all of your team will die if the team is locked",
}

# Additional Chinese-to-English translations for strings that appear in the extracted list
# but are in the format of "X 不透明度" etc. We'll handle these programmatically with common patterns.
_PATTERN_TRANSLATIONS = {
    "不透明度": "Opacity",
    "颜色": "Color",
    "大小": "Size",
    "模式": "Mode",
    "按键": "Key",
    "设置": "Settings",
    "选项": "Options",
    "配置": "Configuration",
    "显示": "Show",
    "隐藏": "Hide",
    "启用": "Enable",
    "禁用": "Disable",
    "切换": "Toggle",
    "发送": "Send",
    "接收": "Receive",
}


def translate_to_english(chinese_str):
    """Translate a Chinese string to English using the manual dictionary.
    Falls back to the original string if no translation is found."""
    # Check exact match first
    if chinese_str in EN_TRANSLATIONS:
        return EN_TRANSLATIONS[chinese_str]
    # Return original if no translation available
    return chinese_str


QMCLIENT_SIMPLIFIED_SOURCE_TRANSLATIONS = {
    '你现在处于单人区域': 'You are now in a solo part',
    '你现在已离开单人区域': 'You are now out of the solo part',
    "'%s' 加入了 0 队": "'%s' joined team 0",
    "队伍存档进行中，之后可以用 '/load %s' 载入": "Team save in progress. You'll be able to load with '/load %s'",
    "队伍存档进行中，成功后可用 '/load %s' 载入，失败时可用 '/load %s' 载入": "Team save in progress. You'll be able to load with '/load %s' if save is successful or with '/load %s' if it fails",
    "队伍已由 %s 成功存档。数据库连接失败，因此改用生成的存档码避免冲突。用 '/load %s' 继续": "Team successfully saved by %s. The database connection failed, using generated save code instead to avoid collisions. Use '/load %s' to continue",
    "队伍已由 %s 成功存档。数据库连接失败，因此改用生成的存档码避免冲突。请在 %s 上用 '/load %s' 继续": "Team successfully saved by %s. The database connection failed, using generated save code instead to avoid collisions. Use '/load %s' on %s to continue",
    "'%s' 关闭了你们队伍的练习模式": "'%s' disabled practice mode for your team",
    "'%s' 锁定了你们的队伍": "'%s' locked your team.",
    "'%s' 锁定了你们的队伍。比赛开始后，任何人 kill 都会导致整队死亡": "'%s' locked your team. After the race starts, killing will kill everyone in your team.",
    "'%s' 解锁了你们的队伍": "'%s' unlocked your team.",
    '这个队伍已经达到最大人数上限 %s': 'This team already has the maximum allowed size of %s players',
    '无法关闭 team 0 模式。该队伍人数已超过普通队伍允许上限 %s': "Can't disable team 0 mode. This team exceeds the maximum allowed size of %s players for regular team",
    "'%s' 关闭了 team 0 模式": "'%s' disabled team 0 mode.",
    "'%s' 开启了 team 0 模式。你们的队伍现在会按 team 0 规则运作": "'%s' enabled team 0 mode. This will make your team behave like team 0.",
    '你当前在 %s 队，队伍里有 %s 人': 'You are in team %s having %s players',
    "'%s' 加入了 %s 队": "'%s' joined team %s",
    "'%s' 邀请你加入 %s 队。输入 /team %s 即可加入": "'%s' invited you to team %s. Use /team %s to join",
    "'%s' 邀请了 '%s' 加入你们的队伍": "'%s' invited '%s' to your team.",
    "这个队伍已无法完赛，因为 '%s' 在碰到起点前离开了队伍": "This team cannot finish anymore because '%s' left the team before hitting the start",
    '你已经向 %s 发过交换请求了': 'You have already requested to swap with %s.',
    '你已向 %s 发出交换请求。输入 /cancelswap 可取消': 'You have requested to swap with %s. Use /cancelswap to cancel the request.',
    '%s 请求与你交换。请等待 %s 秒后输入 /swap %s 完成': '%s has requested to swap with you. To complete the swap process please wait %s seconds and then type /swap %s.',
    '%s 请求与 %s 交换位置': '%s has requested to swap with %s',
    '你还需要等待 %s 秒才能交换': 'You have to wait %s seconds until you can swap',
    '你现在可以跳跃 %s 次': 'You can now jump %s times',
    '救援模式已切换为 %s': 'Rescue mode switched to %s',
    '当前救援模式：%s': 'Current rescue mode: %s',
    '你的交换请求已在 %s 秒前超时，请重新输入 /swap 发起': 'Your swap request timed out %s seconds ago, use /swap again to request',
    '%s 与 %s 已完成交换': '%s and %s have swapped',
    '你已取消与 %s 的交换': 'You canceled the swap with %s',
    '%s 已取消与你的交换': '%s canceled the swap with you',
    '%s 已取消与 %s 的交换': '%s canceled the swap with %s',
    '自杀': 'killed themselves',
    '死亡': 'died',
    "你的锁队全员被处死，因为 '%s' %s 了": "Your locked team was killed because '%s' %s",
    "'%s' 发起了%s队伍练习模式投票。当前票数 %s/%s": "'%s' called vote to %s practice mode for your team. Current votes %s/%s",
    '开启': 'enable',
    "'%s' 不是本服务器可用的投票选项": "'%s' is not a valid option on this server",
    "'%s' 发起了服务器选项投票：%s": "'%s' called vote to change server option: %s",
    "'%s' 发起了服务器选项投票：%s（原因：%s）": "'%s' called vote to change server option: %s (reason: %s)",
    "'%s' 发起了踢出 '%s' 的投票（原因：%s）": "'%s' called for vote to kick '%s' (reason: %s)",
    "'%s' 发起了禁言 '%s' 的投票（原因：%s）": "'%s' called for vote to mute '%s' (reason: %s)",
    "'%s' 发起了暂停 '%s' %s 秒的投票（原因：%s）": "'%s' called for vote to force-pause '%s' for %s seconds (reason: %s)",
    "'%s' 发起了将 '%s' 移到旁观的投票（原因：%s）": "'%s' called for vote to move '%s' to spectators (reason: %s)",
    '每名玩家两次踢人投票之间需要间隔 %s 秒，请再等待 %s 秒': "There's a %s second wait time between kick votes for each player please wait %s second(s)",
    '踢人投票至少需要 %s 名玩家': 'Kick voting requires %s players',
    "授权玩家强制将当前投票设为 '%s'": "Authorized player forced vote '%s'",
    '两次换图投票之间需要间隔 %s 秒，请再等待 %s 秒': "There's a %s second delay between map-votes, please wait %s seconds.",
    "'%s' 发起了针对你的踢人投票": "'%s' called for vote to kick you",
    "'%s' 发起了针对你的旁观投票": "'%s' called for vote to move you to spectators",
    '首次发起投票前还需要等待 %s 秒': 'You must wait %s seconds before making your first vote.',
    '再次发起投票前还需要等待 %s 秒': 'You must wait %s seconds before making another vote.',
    '你接下来 %s 秒内不能发起投票': 'You are not permitted to vote for the next %s seconds.',
    '本服务器启用了初始发言延迟，你还需要等待 %s 秒才能说话': 'This server has an initial chat delay, you will be able to talk in %s seconds.',
    '你接下来 %s 秒内不能发言': 'You are not permitted to talk for the next %s seconds.',
    '计时器当前显示在 %s': 'Timer is displayed in %s',
    '距离下次切换队伍还需等待：%s': 'Time to wait before changing team: %s',
    '你正处于强制暂停状态，还需等待 %s 秒': 'You are force-paused for %s seconds.',
    '你当前的比赛用时是 %s': 'Your current race time is %s',
    '%s 当前的比赛用时是 %s': '%s current race time is %s',
    "正在显示 '%s' 的 checkpoint 时间，当前成绩为 %s": "Showing the checkpoint times for '%s' with a race time of %s",
    "'%s' 原本会超时掉线，但现在可以使用超时保护": "'%s' would have timed out, but can use timeout protection now",
    "'%s' 被强制暂停了 %s 秒": "'%s' was force-paused for %s seconds",
    '栖梦客户端概览': 'QmClient overview',
    '使用顶部标签按分类浏览栖梦功能': 'Use the top tabs to browse QmClient features by category',
    '概览卡展示客户端与页面结构的轻量说明': 'Overview cards show a lightweight guide to the client and page structure',
    '视觉页包含外观和渲染相关选项': 'The Visuals tab contains appearance and rendering options',
    '功能页包含工具、自动化和游戏辅助': 'The Functions tab contains tools, automation, and gameplay helpers',
    '页面指南': 'Page guide',
    '每个标签页都有明确职责': 'Each tab has a clear purpose',
    'HUD 页收集叠加层、计数器、语音显示和顶部组件': 'The HUD tab collects overlays, counters, voice display, and top components',
    '配置页复用栖梦里的客户端配置浏览器': "The Config tab reuses QmClient's client config browser",
    '社区链接、更新与赞助名单已移到贡献者页': 'Community links, updates, and sponsors moved to the Contributors tab',
    '拖拽、折叠、搜索和使用历史会在每个分类中保留': 'Dragging, collapsing, search, and usage history are preserved per category',
    '栖梦社区': 'QmClient Community',
    '官方社区链接': 'Official community links',
    'QQ群: 1076765929（点击复制）': 'QQ group: 1076765929 (click to copy)',
    '点击复制QQ群号': 'Click to copy QQ group number',
    '加入QQ群': 'Join QQ group',
    '赞助支持': 'Sponsor support',
    '感谢支持栖梦客户端': 'Thanks for supporting QmClient',
    '隐藏赞助码': 'Hide sponsor QR code',
    '显示赞助码': 'Show sponsor QR code',
    '无法加载支持二维码。请检查 Base64 内容': 'Could not load sponsor QR code. Check the Base64 data',
    '支持二维码的 Base64 内容未配置': 'Sponsor QR code Base64 data is not configured',
    '查看最新更新': 'View latest updates',
    '栖梦客户端': 'QmClient',
    '开发以及赞助者': 'Developers and sponsors',
    '赞助者:': 'Sponsors:',
    '皮肤切换': 'Skin transition',
    '调整锤中偷皮和换皮动画': 'Configure hammer skin steal and skin transition animations',
    '调试图表': 'Debug graph',
    '调试性能图表面板': 'Debug performance graph panel',
    '持续时间': 'Duration',
    '字体大小': 'Font size',
    '自动切换快速输入': 'Auto-toggle fast input',
    '自动切换快速输入（其他人）': 'Auto-toggle fast input others',
    '主动断开': 'Active disconnect',
    '显示Qm标识': 'Show Qm badge',
    '新版IME': 'New IME',
    '换皮动画类型': 'Skin transition type',
    '残影弹出': 'Afterimage pop',
    '柔和淡变': 'Smooth fade',
    '向左滑切': 'Slide left',
    '旋转弹出': 'Spin pop',
    '明暗切换': 'Brightness shift',
    '换皮动画时长': 'Skin transition duration',
    '刷新间隔': 'Refresh interval',
    '看起来已经是目标语言的消息会跳过自动翻译': 'Messages that already look like the target language will skip auto-translate',
    '纯数字消息会直接跳过': 'Numeric-only messages will be skipped',
    '地域': 'Region',
    'API 密钥': 'API key',
    '智谱 API 密钥': 'Zhipu API key',
    'DeepSeek API 密钥': 'DeepSeek API key',
    'OpenAI API 密钥': 'OpenAI API key',
    '自定义 API 密钥': 'Custom API key',
    '启用思考模式需要使用推理模型': 'Thinking mode requires a reasoning model',
    '确保后端支持 OpenAI 兼容的思考参数': 'Make sure the backend supports OpenAI-compatible thinking parameters',
    '自动回复冷却时间': 'Auto reply cooldown',
    'UI 比例': 'UI scale',
    '提及': 'Mention',
    '复制皮肤': 'Copy skin',
    '切换': 'Switch',
    '仅自己': 'Self only',
    '本地': 'Local',
    '所有人': 'All players',
    '动画范围': 'Animation range',
    '激光风格': 'Laser style',
    '激光效果增强': 'Laser effect enhancement',
    '脉冲速度': 'Pulse speed',
    '脉冲幅度': 'Pulse amplitude',
    '玩家数据': 'Player data',
    '玩家统计数据和信息显示': 'Player stats and info display',
    '显示玩家统计数据HUD': 'Show player stats HUD',
    '地图进度条': 'Map progress bar',
    '竖直位置': 'Vertical position',
    '未知': 'Unknown',
    '古典.easy': 'Classic easy',
    '古典.next': 'Classic next',
    '古典.pro': 'Classic pro',
    '古典.nut': 'Classic nut',
    '古典': 'Classic',
    '简单': 'Novice',
    '中阶': 'Moderate',
    '高阶': 'Brutal',
    '疯狂': 'Insane',
    '单人': 'Solo',
    '传统': 'Oldschool',
    '竞速': 'Race',
    '娱乐': 'Fun',
    '活动': 'Event',
    '从收藏中移除': 'Remove from favorites',
    '解冻时自动取消旁观': 'Auto unspec on unfreeze',
    '速通倒计时器': 'Speedrun countdown timer',
    '小时': 'Hours',
    '分钟': 'Minutes',
    '秒': 'Seconds',
    '毫秒': 'Milliseconds',
    '时间到时自动禁用': 'Auto disable when time expires',
    '全局开关键': 'Global toggle key',
    '面板不透明度': 'Panel opacity',
    '输入叠加': 'Input overlay',
    '输入叠加显示': 'Input overlay display',
    '配置文件: data/input_overlay.json': 'Config file: data/input_overlay.json',
    '外部保存后自动热重载': 'Auto hot-reload after external saves',
    '通知栏': 'Notifications',
    '把 Echo 和需要关注的系统提示移到右侧弹出显示': 'Move Echo and important system prompts to right-side popups',
    '服务器系统提示改走通知栏（黑名单除外）': 'Route server system prompts to notifications (except blacklist)',
    'Echo 消息改走通知栏': 'Route Echo messages to notifications',
    '其他服务器的类似提示也尝试改走通知栏（例如自定义单人区域提示，按黑名单排除）': 'Also route similar prompts from other servers to notifications (for example custom solo prompts; blacklist still applies)',
    '通知栏背景颜色': 'Notification background',
    '系统提示文字颜色': 'System prompt text',
    'Echo 跟随聊天里当时的实际颜色': 'Echo follows the original chat color',
    'Echo 不跟随聊天颜色时的文字颜色': 'Echo text color when not inheriting chat color',
    '通知文字大小': 'Notification text size',
    '每条显示多久': 'Notification hold time',
    '弹出动画': 'Popup animation',
    '淡入滑入': 'Fade and slide',
    '仅淡入': 'Fade only',
    '无动画': 'No animation',
    '动画持续多久': 'Animation duration',
    '最多同时显示几条': 'Max visible notifications',
    '粒子辉光': 'Particle glow',
    '粒子脉冲': 'Particle pulse',
    '无效的 ID': 'Invalid ID',
    '该 ID 未连接': 'This ID is not connected',
    '没有可翻译的消息': 'No chat message to translate',
    '不翻译服务器消息': 'Do not translate server messages',
    '无效的翻译后端': 'Invalid translation backend',
    '%s 正在翻译为 %s': '%s translating to %s',
    '翻译任务过多': 'Too many translation tasks',
    '%s 正在发送前翻译为 %s': '%s translating to %s before send',
    '%s 翻译为 %s 失败: %s': '%s translating to %s failed: %s',
    'FTAPI 自动翻译已禁用以避免服务过载。需要时可在设置中启用。': 'FTAPI auto-translate is disabled to prevent overload. Enable in settings if needed.',
    '翻译队列已满，发送原文': 'Translation queue full, sending original',
    '翻译后端无效，发送原文': 'Translation backend invalid, sending original',
    '正在翻译为 %s...': 'Translating to %s...',
    '按钮 - 禁用': 'Button - Disabled',
    '按钮 - 启用': 'Button - Enabled',
    '菜单背景': 'Menu background',
    '菜单选项 - 选中': 'Menu option - selected',
    '菜单选项 - 普通': 'Menu option - normal',
    '自由旁观光标不透明度': 'Freeview cursor opacity',
    '旁观选择中显示颜色': 'Show colors in spectator selection',
    'a = 视角角度': 'a = View angle',
    'p = Ping 延迟': 'p = Ping latency',
    'd = 预测延迟': 'd = Prediction latency',
    'c = 玩家坐标': 'c = Player position',
    'l = 本地时间': 'l = Local time',
    'r = 比赛时间': 'r = Race time',
    'f = 帧率': 'f = Frame rate',
    'v = 速度': 'v = Velocity',
    'z = 缩放': 'z = Zoom',
    'u = 快照延迟': 'u = Snapshot latency',
    'n = 预测延迟': 'n = Prediction latency',
    'j = 延迟抖动': 'j = Latency jitter',
    'k = 重发丢包率': 'k = Resend loss',
    'i = 接收速率': 'i = Receive rate',
    'o = 发送速率': 'o = Send rate',
    'q = 连接质量': 'q = Connection quality',
    'x = DDNet CPU% / 总 CPU%': 'x = DDNet CPU% / total CPU%',
    'y = DDNet 内存占用': 'y = DDNet memory usage',
    "_ 或 ' ' = 空白间隔": "_ or ' ' = blank spacer",
    '栖梦': 'QmClient',
    '正在尝试 Axiom 自动登录': 'Trying Axiom auto login',
    'Axiom 自动登录成功': 'Axiom auto login succeeded',
    'Axiom 自动登录失败，正在重试': 'Axiom auto login failed, retrying',
    'Axiom 自动登录失败': 'Axiom auto login failed',
    '运行 chai 脚本': 'Run chai script',
    '正在尝试 Axiom 分身自动登录': 'Trying Axiom dummy auto login',
    '更新提示': 'Update notice',
    '你已经是最新版本': 'You are already on the latest version',
    '当前版本不是最新版，请前往 QQ 群更新最新版': 'Your current version is outdated. Please update from the QQ group.',
    '关闭': 'disable',
    '临时自由镜头': 'Temporary free camera',
    '按住左键自由镜头': 'Hold left click for free camera',
    '直播导播': 'Live director',
    '%d 个队伍': '%d teams',
    '%d 个玩家': '%d players',
    '暂无可导播玩家': 'No director players available',
    '%c 队伍 %d': '%c Team %d',
    '- 您在这张图有%d个存档！': '- You have %d saves on this map!',
    '- 保存者按顺序是:': '- Save owners in order:',
    '- 密码依次为:': '- Save codes in order:',
    '你的队伍已被解锁队伍图块解除锁定': 'Your team was unlocked by an unlock team tile',
    '发送时在末尾追加 [ru]、[en]、[ja] 等语言代码': 'Append language codes like [ru], [en], [ja] at the end when sending',
    '自动翻译会跳过简体中文、繁体中文和服务器消息': 'Auto-translate will skip simplified Chinese, traditional Chinese, and server messages',
    '缓冲内存': 'Buffer memory',
    '客户端 CPU 占用偏高': 'Client CPU usage is high',
    '客户端帧时间异常': 'Client frame time is abnormal',
    '客户端性能压力偏高': 'Client performance pressure is high',
    '客户端预测耗时偏高': 'Client prediction time is high',
    '连接断开': 'Connection disconnected',
    '连接下行': 'Connection downstream',
    '连接偏高': 'Connection elevated',
    '连接正常': 'Connection normal',
    '连接严重异常': 'Connection severely abnormal',
    '连接上行': 'Connection upstream',
    'DDNet/总 CPU': 'DDNet/total CPU',
    'DeepSeek': 'DeepSeek',
    '偏高': 'Elevated',
    '展开模块': 'Expand module',
    '帧时间': 'Frame time',
    '游戏/预测 Tick': 'Game/predicted tick',
    '隐藏 Echo 消息': 'Hide Echo messages',
    '隐藏死亡/重生特效': 'Hide death/respawn effects',
    '隐藏方向': 'Hide direction indicators',
    '隐藏炮击特效': 'Hide explosion effects',
    '隐藏冻结特效': 'Hide freeze effects',
    '隐藏锤击特效': 'Hide hammer effects',
    '隐藏入场/版本提示': 'Hide join/version prompts',
    '隐藏跳跃特效': 'Hide jump effects',
    '隐藏击杀/完成': 'Hide kill/finish messages',
    '隐藏地图进度': 'Hide map progress',
    '隐藏名字板': 'Hide nameplates',
    '隐藏名字': 'Hide names',
    '隐藏玩家消息': 'Hide player messages',
    '隐藏记分板': 'Hide scoreboard',
    '隐藏服务器提示通知': 'Hide server prompt notifications',
    '隐藏武器火焰': 'Hide weapon muzzle flashes',
    '界面': 'Interface',
    '抖动': 'Jitter',
    '延迟': 'Latency',
    '内存': 'Memory',
    '最小化模块': 'Minimize module',
    '静音死亡/重生': 'Mute death/respawn sounds',
    '静音锤击': 'Mute hammer sounds',
    '静音跳跃': 'Mute jump sounds',
    '暂无明显异常': 'No obvious anomaly',
    '当前未连接到游戏服务器': 'Not connected to a game server',
    '只有 Apple 才能做到': 'Only Apple Can Do',
    'OpenAI': 'OpenAI',
    '切换武器时播放滑入旋转动画': 'Play a slide-in rotation animation when switching weapons',
    '预测波动明显，延迟变化较大': 'Prediction jitter is obvious, latency changes are large',
    '预测值抬升，预测链路压力偏高': 'Prediction latency is elevated, prediction path pressure is high',
    '预测耗时': 'Prediction time',
    '接收': 'Receive',
    '存在重发迹象，链路质量可疑': 'Resend signs detected, connection quality is suspicious',
    '回拉率': 'Rollback rate',
    '服务器 RTT 抬升，回包链路波动较明显': 'Server RTT is elevated, response path is unstable',
    '时间回拉': 'Server rollback',
    '严重': 'Severe',
    '系统媒体控制': 'SMTC',
    '暂存内存': 'Staging memory',
    '流式内存': 'Streamed memory',
    '腾讯云 SecretId': 'Tencent Cloud SecretId',
    '腾讯云 SecretKey': 'Tencent Cloud SecretKey',
    '纹理内存': 'Texture memory',
    'SecretId': 'SecretId',
    'SecretKey': 'SecretKey',
    '武器动画': 'Weapon animation',
    '武器切换动画': 'Weapon switch animation',
}

def read_strings():
    """Read extracted strings from file."""
    with open(STRINGS_FILE, "r", encoding="utf-8") as f:
        strings = [line.rstrip("\n") for line in f if line.strip()]
    return strings


def write_language_file(filename, entries):
    """Write a language file in the DDNet format: key then == translation."""
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    filepath = os.path.join(OUTPUT_DIR, filename)
    with open(filepath, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n\n".join(f"{key}\n== {translation}" for key, translation in entries))
        if entries:
            f.write("\n")
    print(f"  Wrote {len(entries)} entries to {filepath}")


def read_language_keys(filename):
    """Read DDNet-style language keys from a file."""
    keys = set()
    if not os.path.exists(filename):
        return keys
    with open(filename, "r", encoding="utf-8-sig") as f:
        for raw_line in f:
            line = raw_line.rstrip("\n")
            if line.endswith("\r"):
                line = line[:-1]
            if not line or line.startswith("#") or line.startswith("[") or line.startswith("== "):
                continue
            keys.add(line)
    return keys


def generate_english(strings):
    """English falls back to the source key, so no additive file is needed."""
    return []


def generate_simplified_chinese(strings):
    """
    For simplified_chinese.txt: English keys → Simplified Chinese translations.
    DDNet's base simplified_chinese.txt is loaded first, so this overlay only
    needs QmClient-specific entries that the base file does not already cover.
    """
    current_keys = set(strings)
    base_keys = read_language_keys(BASE_SIMPLIFIED_CHINESE)
    english_to_chinese = {}
    for chinese, english in EN_TRANSLATIONS.items():
        english_to_chinese.setdefault(english, chinese)
    for source_translations in (
        QMCLIENT_SIMPLIFIED_SOURCE_TRANSLATIONS,
        QMCLIENT_SIMPLIFIED_STATIC_NOTIFICATION_TRANSLATIONS,
    ):
        for chinese, english in source_translations.items():
            english_to_chinese[english] = chinese

    entries = []
    missing_translations = []
    for s in strings:
        if is_chinese(s) or s in base_keys:
            continue
        translated = english_to_chinese.get(s)
        if translated is not None:
            if translated != s:
                entries.append((s, translated))
        else:
            missing_translations.append(s)
    if missing_translations:
        print("  WARNING: missing Simplified Chinese translations for English keys:")
        for key in missing_translations:
            print(f"    - {key}")
    return entries


def generate_other_language(strings, filename):
    """
    For other languages: use English translations as placeholder.
    Community translators can replace these later.
    DDNet's base language is loaded first, so do not override keys that the
    base language already translates.
    """
    base_keys = read_language_keys(os.path.join(BASE_LANGUAGES_DIR, f"{filename}.txt"))
    entries = []
    for s in strings:
        if not is_chinese(s) and s not in base_keys:
            entries.append((s, s))
    return entries


def create_readme():
    """Create a README explaining the translation structure."""
    readme_path = os.path.join(OUTPUT_DIR, "README.txt")
    with open(readme_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("""QmClient Translation Files
=========================

These files contain translations for QmClient-specific UI strings.
They are loaded additively on top of DDNet's base translations.

File format:
  key
  == translation

- Keys are English source strings from QmClient UI code.
- English users normally rely on Localize() fallback, so english.txt can be empty or absent.

How to contribute translations:
  1. Open the file for your language (e.g., german.txt).
  2. Find entries where the translation matches the English fallback.
  3. Replace the English text with your language's translation.
  4. Keep the "== " prefix before each translation.
  5. Do NOT modify the key line (the line without "== ").

Example:
  Before:
    Chat Bubble
    == Chat Bubble

  After (German):
    Chat Bubble
    == Chat-Blase

Current status:
  - Source code uses English keys, matching DDNet's base localization model
  - Simplified Chinese environment: `simplified_chinese.txt` maps English keys to Chinese
  - English environment: falls back to source English keys
  - All other languages: English placeholder translations until localized

Auto-generated by qmclient_scripts/languages_qmclient/generate_all.py
""")
    print(f"  Created {readme_path}")


def create_index():
    """Create data/qmclient/languages/index.txt (same format as DDNet's)."""
    index_path = os.path.join(OUTPUT_DIR, "index.txt")
    with open(index_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n\n".join(f"{filename}\n== {native_name}\n== {country_code}\n== {lang_tags}" for filename, native_name, country_code, lang_tags in LANGUAGES))
        f.write("\n")
    print(f"  Created {index_path}")


def main():
    print("=" * 60)
    print("QmClient Translation File Generator")
    print("=" * 60)

    # Read extracted strings
    strings = read_strings()
    print(f"\nLoaded {len(strings)} unique strings")

    # Create output directory
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Create index file
    print("\n--- Creating index.txt ---")
    create_index()

    # Create README
    print("\n--- Creating README ---")
    create_readme()

    # Generate language files
    for filename, native_name, country_code, lang_tags in LANGUAGES:
        print(f"\n--- {filename}.txt ({native_name}) ---")
        if filename == "english":
            entries = generate_english(strings)
        elif filename == "simplified_chinese":
            entries = generate_simplified_chinese(strings)
        else:
            entries = generate_other_language(strings, filename)

        write_language_file(f"{filename}.txt", entries)

    print(f"\n{'=' * 60}")
    print("Done! All language files generated in:")
    print(f"  {OUTPUT_DIR}")
    print(f"{'=' * 60}")

    # Print summary
    chinese_keys = sum(1 for s in strings if is_chinese(s))
    english_keys = len(strings) - chinese_keys
    print("\nSummary:")
    print(f"  Legacy Chinese keys: {chinese_keys}")
    print(f"  English source keys: {english_keys}")
    print(f"  Total unique strings: {len(strings)}")
    print(f"  Language files created: {len(LANGUAGES)}")
    print(
        f"  Simplified Chinese source translations provided: "
        f"{len(EN_TRANSLATIONS) + len(QMCLIENT_SIMPLIFIED_SOURCE_TRANSLATIONS) + len(QMCLIENT_SIMPLIFIED_STATIC_NOTIFICATION_TRANSLATIONS)}"
    )


if __name__ == "__main__":
    main()
