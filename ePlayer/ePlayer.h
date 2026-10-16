#pragma once
#include "MainWindow.h"
#include <atomic>


class ePlayer
{
public:
	std::atomic<int> isQuit = 0;

public:
	ePlayer(const ePlayer& player) = delete;
	ePlayer& operator=(const ePlayer& player) = delete;
	ePlayer();

	static ePlayer& Get();
	
	bool Init(const char* fileName);
	bool OnPlay();
	bool Quit();

private:
	MainWindow* window = nullptr;
	
	const char* fileName = NULL;

private:


};

