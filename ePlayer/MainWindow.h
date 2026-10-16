#pragma once

#include <SDL2\SDL.h>
#include <atomic>
#include "Codec.h"
#include "FrameQueue.h"


class MainWindow
{
public:

public:
	MainWindow(std::atomic<int>& quit);
	~MainWindow();
	

	//window
	bool CreateWindow();
	bool InitCodec(const char* fileName);
	bool CloseWindow() const;

	bool Loop();

private:

	int windowWidth = 1280;
	int windowHeight = 720;
	const char* title = "ePlayer";

	char* fileName = NULL;

	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	SDL_Texture* texture = nullptr;

	//codec
	Codec* codec = nullptr;

	std::atomic<int>& isQuit;

	FrameQueue frameQueue = {};

private:

	bool CreateTexture();
	bool CreateRenderer();

	void OnRender();

};

