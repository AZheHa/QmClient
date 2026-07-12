// 请抬头享受阳光｜日子很好 我很我---------致咩子
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_QMTREE_H
#define GAME_CLIENT_QMUI_QMTREE_H

#include <cstdint>
#include <unordered_set>

class CUiV2Tree
{
public:
	void Reset();
	void BeginFrame();
	void TouchNode(uint64_t NodeKey);
	void EndFrame();
	int NodeCount() const;

private:
	std::unordered_set<uint64_t> m_FrameNodeKeys;
	int m_LastNodeCount = 0;
};

#endif
