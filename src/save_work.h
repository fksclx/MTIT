#pragma once
static void SaveStop();
static int SaveStart();
static void SaveError();
static void saveFrame();
int clearSaveData();
void* save_run(void *arg);
static void recvMainCMDSignal();
static void recvSaveDataSignal();


