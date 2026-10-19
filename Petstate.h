// SPDX-License-Identifier: MIT
#pragma once	// 防止这个头文件在同一个 .cpp 里被重复包含

// 宠物的 4 种状态。用 enum class（强类型枚举）：
// 使用时必须写 PetState:: 前缀，也不会自动转换成 int，避免和数字混用
enum class PetState
{
	Idle,				// 空闲：程序刚启动、Claude 回复完毕（Stop）时
	Working,			// 工作中：你发出消息、Claude 调用工具时
	NeedsInput,			// 需要你介入：Claude 申请权限（PermissionRequest）、或向你提问时
	Error,				// 出错：API 出错（StopFailure）时
};