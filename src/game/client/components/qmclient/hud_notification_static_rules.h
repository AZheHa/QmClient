#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATION_STATIC_RULES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATION_STATIC_RULES_H

// Compatibility layer for pre-semantic static categories that are still consumed by
// hud_notification_rules.cpp. The semantic upstream/alias tables are the canonical
// source for the migrated static families; do not reintroduce a mixed total-table macro here.

#define QM_HUD_NOTIFICATION_STATIC_TEAM_RULES(X) \
	X("Team save disabled for teams in practice mode", "练习模式下不能保存队伍存档") \
	X("Team load already in progress", "队伍读档已在进行中") \
	X("You have to be in a team (from 1-63)", "你必须在队伍中（1 到 63 队）") \
	X("Team can't be loaded while racing", "比赛进行中时不能读档") \
	X("Team can't be loaded while in team 0 mode", "处于 0 队模式时不能读档") \
	X("Team can't be loaded while practice is enabled", "开启练习模式时不能读档") \
	X("Could not find your Team", "找不到你的队伍") \
	X("To save all players in your team have to be alive and not in '/spec'", "要保存队伍，队内所有玩家都必须存活且不能处于 '/spec'") \
	X("Your team has not started yet", "你的队伍还没有开始") \
	X("Team can't be saved while in team 0 mode", "处于 0 队模式时不能保存队伍存档") \
	X("Team can't be saved while a dragger is active", "有拖拽器生效时不能保存队伍存档") \
	X("Your team was killed because it couldn't finish anymore and hasn't entered /practice mode", "你的队伍因已无法完赛且未进入 /practice 模式而被处死") \
	X("This team started already", "这个队伍已经开始比赛了") \
	X("You are in this team already", "你已经在这个队伍里了") \
	X("You can't change teams while you are dead/a spectator.", "你死亡或处于旁观状态时，不能切换队伍。") \
	X("You can't join super team if you don't have super rights", "没有 super 权限时不能加入 super 队伍") \
	X("You have started racing already", "你已经开始比赛了") \
	X("You have used practice mode already", "你已经使用过练习模式了") \
	X("This team is currently saving", "这个队伍当前正在存档") \
	X("Your team is currently saving", "你的队伍当前正在存档") \
	X("Start holding the hook before loading the savegame to keep the hook", "如果想保留钩子状态，请在读档前先按住钩子") \
	X("Your team has been killed because it contains an invalid tee state", "你的队伍因包含无效 tee 状态而被处死") \
	X("You died, but will stay in practice until you use kill.", "你已经死亡，但会继续保持练习模式，直到你输入 kill") \
	X("This team was disbanded because there are more players than allowed in the team.", "该队伍因人数超过允许上限而被解散") \
	X("你的队伍已被解锁队伍图块解除锁定", "你的队伍已被解锁队伍图块解除锁定") \
	X("Enter /practice mode or restart to avoid the entire team being killed in 60 seconds", "请输入 /practice 或重新开始，否则整队将在 60 秒后被处死") \
	X("Join a team to enable practice mode, which means you can use /r, but can't earn a rank.", "先加入队伍才能开启练习模式。开启后可以使用 /r，但不会获得排名") \
	X("Practice mode can't be enabled in team 0 mode.", "0 队模式下不能开启练习模式") \
	X("Practice mode can't be enabled while team save or load is in progress", "队伍正在存档或读档时，不能开启练习模式") \
	X("Team is already in practice mode", "队伍已经处于练习模式") \
	X("Practice mode enabled for your team, happy practicing!", "你的队伍已开启练习模式，祝你练习愉快！") \
	X("This team can't be locked", "这个队伍不能被锁定") \
	X("Teams are disabled", "队伍功能已禁用") \
	X("Invites are disabled", "本服务器已禁用队伍邀请") \
	X("/map is disabled", "本服务器已禁用 /map") \
	X("Practice mode is disabled", "本服务器已禁用练习模式") \
	X("Save-function is disabled on this server", "本服务器已禁用存档功能") \
	X("You must join a team and play with somebody or else you can't play", "你必须加入一个队伍并和其他人一起玩，否则无法开始") \
	X("No empty team left.", "已经没有空队伍了") \
	X("You can't change teams that fast!", "你切换队伍太快了") \
	X("This team is locked using /lock. Only members of the team can unlock it using /lock.", "这个队伍已用 /lock 锁定，只有队伍成员才能用 /lock 解锁") \
	X("This team is locked using /lock. Only members of the team can invite you or unlock it using /lock.", "这个队伍已用 /lock 锁定，只有队伍成员才能邀请你或用 /lock 解锁") \
	X("Player not found", "未找到该玩家") \
	X("Player already invited", "该玩家已经被邀请过了") \
	X("Can't invite this quickly", "你发送邀请太快了，请稍后再试") \
	X("Can't invite players to this team", "当前这个队伍不能邀请玩家") \
	X("Team mode change disabled", "当前不允许切换队伍模式") \
	X("Team mode change is disabled on this server.", "本服务器已禁用队伍模式切换") \
	X("This team can't have the mode changed", "这个队伍不能切换模式") \
	X("Team mode can't be changed while racing", "比赛进行中时不能切换队伍模式")

#define QM_HUD_NOTIFICATION_STATIC_SWAP_RESCUE_RULES(X) \
	X("Unknown argument. Check '/rescuemode list'", "未知救援模式参数") \
	X("There is nowhere to go back to.", "没有可回退的位置") \
	X("You're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.", "你当前不在开启了 /practice 的队伍中。注意：练习模式下无法获得排名") \
	X("You haven't previously teleported. Use /tp before using this command.", "你之前没有传送过，请先使用 /tp") \
	X("There is no teleporter with that index on the map.", "这张地图上没有这个编号的传送器") \
	X("There is no checkpoint teleporter with that index on the map.", "这张地图上没有这个编号的 checkpoint 传送器") \
	X("Can't enable team 0 mode with practice mode on.", "开启练习模式时不能启用 team 0 模式") \
	X("Can't swap with yourself", "你不能和自己交换位置") \
	X("Player is on a different team", "目标玩家不在你的队伍里") \
	X("You and other player need to have started the map", "你和对方都需要先开始地图，才能交换位置") \
	X("Need to have started the map to swap with a player.", "你需要先开始地图，才能和其他玩家交换位置") \
	X("You and the other player must not be paused.", "你和对方都不能处于暂停状态，才能交换位置") \
	X("Swap is disabled on this server.", "本服务器已禁用交换功能") \
	X("Swap is not available on forced solo servers.", "强制 solo 服务器上不能使用交换功能") \
	X("Join a team to use swap feature, which means you can swap positions with each other.", "先加入队伍后才能使用交换功能，也就是和队友互换位置") \
	X("You do not have a pending swap request.", "你当前没有待处理的交换请求")

#define QM_HUD_NOTIFICATION_STATIC_VOTE_MODERATION_RULES(X) \
	X("You are running a vote, please try again after the vote is done!", "你正在发起投票，请等当前投票结束后再试") \
	X("Invalid option", "无效的投票选项") \
	X("Server does not allow voting to kick players", "本服务器不允许发起踢人投票") \
	X("Invalid client id to kick", "用于踢人的客户端 ID 无效") \
	X("You can't kick yourself", "你不能踢自己") \
	X("You can't kick authorized players", "你不能踢已授权玩家") \
	X("You can kick only your team member", "你只能踢自己队伍里的成员") \
	X("Server does not allow voting to move players to spectators", "本服务器不允许发起移至旁观投票") \
	X("Invalid client id to move to spectators", "用于移至旁观的客户端 ID 无效") \
	X("You can't move yourself to spectators", "你不能把自己移到旁观") \
	X("You can't move authorized players to spectators", "你不能把已授权玩家移到旁观") \
	X("You can only move your team member to spectators", "你只能把自己队伍里的成员移到旁观") \
	X("Kill Protection enabled. If you really want to join the spectators, first type /kill", "已启用防自杀保护。如果你确实想进入旁观，请先输入 /kill") \
	X("You can only vote after logging in.", "登录后才可以发起投票") \
	X("You are not allowed to vote because we're currently checking for VPNs. Try again in ~30 seconds.", "当前正在检查你的 VPN 状态，约 30 秒后再尝试发起投票") \
	X("You are not allowed to vote because you appear to be using a VPN. Try connecting without a VPN or contacting an admin if you think this is a mistake.", "你当前看起来正在使用 VPN，暂时不能发起投票。如有误判，请关闭 VPN 或联系管理员") \
	X("Wait for current vote to end before calling a new one.", "请先等待当前投票结束，再发起新的投票")

#define QM_HUD_NOTIFICATION_STATIC_STATUS_RULES(X) \
	X("Unknown parameter. Accepted values: default, gametimer, broadcast, both, none", "未知参数。可用值：default、gametimer、broadcast、both、none") \
	X("Selected timertype is not supported by your client", "你当前客户端不支持所选计时器类型") \
	X("Timer isn't displayed.", "计时器当前不显示") \
	X("Active moderator mode enabled for you.", "已为你开启主动管理员模式") \
	X("Active moderator mode disabled for you.", "已为你关闭主动管理员模式") \
	X("Server kick/spec votes will now be actively moderated.", "服务器的踢人/旁观投票现在会被主动管理员模式接管") \
	X("Server kick/spec votes are no longer actively moderated.", "服务器的踢人/旁观投票已不再由主动管理员模式接管") \
	X("You can see other players. To disable this use DDNet client and type /showothers", "你当前可以看到其他玩家") \
	X("Active moderator mode disabled because you are afk.", "由于你已挂机，主动管理员模式已关闭") \
	X("The force pause timer is now over, you can exit with /spec", "强制暂停计时已结束，你现在可以用 /spec 退出") \
	X("Can't /spec that quickly.", "你不能这么快再次 /spec") \
	X("Invalid spectator id used", "无效的旁观目标 ID") \
	X("Players are not allowed to chat from VPNs at this time", "当前使用 VPN 的玩家不允许发言") \
	X("You can't check your team while you are dead/a spectator.", "死亡或旁观时不能查看当前队伍") \
	X("Showing the team top 5 is not allowed on this server.", "本服务器不允许查看队伍前 5 名") \
	X("Showing the top is not allowed on this server.", "本服务器不允许查看排行榜") \
	X("Showing the times of others is not allowed on this server.", "本服务器不允许查看其他玩家的成绩") \
	X("Showing the team rank of other players is not allowed on this server.", "本服务器不允许查看其他玩家的队伍排名") \
	X("Showing the rank of other players is not allowed on this server.", "本服务器不允许查看其他玩家的排名") \
	X("Showing the global points of other players is not allowed on this server.", "本服务器不允许查看其他玩家的全局积分") \
	X("Showing the global top points is not allowed on this server.", "本服务器不允许查看全局积分排行榜") \
	X("Showing the checkpoint times is not allowed on this server.", "本服务器不允许查看 checkpoint 时间") \
	X("Showing players from other teams is disabled", "本服务器已禁用显示其他队伍玩家") \
	X("Teams are available on this server ；队伍上锁后，队内任意玩家死亡都会导致全队死亡", "本服务器允许组队；队伍上锁后，队内任意玩家死亡都会导致全队死亡") \
	X("Teams are not available on this server ；队伍上锁后，队内任意玩家死亡都会导致全队死亡", "本服务器不允许组队；队伍上锁后，队内任意玩家死亡都会导致全队死亡") \
	X("You have to be in a team to play on this server and all of your team will die if the team is locked", "你必须加入队伍才能在本服务器游玩；队伍上锁后，队内任意玩家死亡都会导致全队死亡") \
	X("Players can collide on this server", "本服务器允许玩家碰撞") \
	X("Players can't collide on this server", "本服务器不允许玩家碰撞") \
	X("Players can hook each other on this server", "本服务器允许玩家互钩") \
	X("Players can't hook each other on this server", "本服务器不允许玩家互钩") \
	X("Scores are private on this server", "本服务器的成绩是私密的") \
	X("Scores are public on this server", "本服务器的成绩是公开的") \
	X("You will not receive any further global chat and server messages", "你将不再接收全局聊天和服务器消息") \
	X("You will receive global chat and server messages", "你将继续接收全局聊天和服务器消息") \
	X("Command is not available on solo servers", "这个命令在 solo 服务器上不可用") \
	X("Emotes are disabled.", "本服务器已禁用表情功能") \
	X("You can now use the preset eye emotes.", "你现在可以使用预设眼部表情") \
	X("You don't have any eye emotes, remember to bind some.", "你当前没有可用的眼部表情，记得先绑定") \
	X("No player with this name found.", "没有找到这个名字的玩家") \
	X("Invalid X coordinate.", "无效的 X 坐标") \
	X("Invalid Y coordinate.", "无效的 Y 坐标") \
	X("Can't recognize specified arguments. Usage: /tpxy x y, e.g. /tpxy 9 3.", "无法识别 /tpxy 参数") \
	X("You can't hit others", "你现在不能攻击其他玩家") \
	X("You can hit others", "你现在可以攻击其他玩家") \
	X("You can't collide with others", "你现在不能与其他玩家碰撞") \
	X("You can collide with others", "你现在可以与其他玩家碰撞") \
	X("You can't hook others", "你现在不能钩中其他玩家") \
	X("You can hook others", "你现在可以钩中其他玩家") \
	X("You have unlimited air jumps", "你现在拥有无限空跳") \
	X("You don't have unlimited air jumps", "你现在没有无限空跳了") \
	X("You have a jetpack gun", "你现在拥有喷气背包枪") \
	X("You lost your jetpack gun", "你失去了喷气背包枪") \
	X("Teleport gun enabled", "传送枪已开启") \
	X("Teleport gun disabled", "传送枪已关闭") \
	X("Teleport grenade enabled", "传送手雷已开启") \
	X("Teleport grenade disabled", "传送手雷已关闭") \
	X("Teleport laser enabled", "传送激光已开启") \
	X("Teleport laser disabled", "传送激光已关闭") \
	X("You can hammer hit others", "你现在可以用锤子攻击其他玩家") \
	X("You can't hammer hit others", "你现在不能用锤子攻击其他玩家") \
	X("You can shoot others with shotgun", "你现在可以用散弹枪攻击其他玩家") \
	X("You can't shoot others with shotgun", "你现在不能用散弹枪攻击其他玩家") \
	X("You can shoot others with grenade", "你现在可以用手雷攻击其他玩家") \
	X("You can't shoot others with grenade", "你现在不能用手雷攻击其他玩家") \
	X("You can shoot others with laser", "你现在可以用激光攻击其他玩家") \
	X("You can't shoot others with laser", "你现在不能用激光攻击其他玩家") \
	X("Endless hook has been activated", "无限钩已开启") \
	X("Endless hook has been deactivated", "无限钩已关闭")

#endif
