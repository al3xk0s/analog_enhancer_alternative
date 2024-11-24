#include <vitasdkkern.h>
#include <taihen.h>
#include <libk/string.h>
#include <libk/stdlib.h>
#include <math.h>

#define HOOKS_NUM   3  // Hooked functions num

static uint8_t current_hook = 0;
static SceUID hooks[HOOKS_NUM];
static tai_hook_ref_t refs[HOOKS_NUM];

static int MIDDLE = 127;
static int END_RANGE = 255;

static uint8_t apply_wide_patch = 0;

struct deadZone {
	int start;
	int end;
};

typedef struct deadZone DeadZone;

struct analogValues {
	int x;
	int y;
};

typedef struct analogValues AnalogValues;


int getPatchedStickValue(int value, DeadZone* deadZone) {
	int result = calcPatchedStickValue(value, deadZone);
    if(result < 0) return 0;
    if(result > END_RANGE) return END_RANGE;

    return result;
}

int calcPatchedStickValue(int value, DeadZone* deadZone) {
	if(value < (MIDDLE + deadZone->start) && value > (MIDDLE - deadZone->start)) return MIDDLE;

    float processValue = (float)value;	
	int maxDeadZonedValue = END_RANGE - deadZone->end;
    
	float scaleFactor = (float)END_RANGE / maxDeadZonedValue;
    
	if(value < 127) return (int)(processValue / scaleFactor);
	return (int)(processValue * scaleFactor);
}

AnalogValues getPatchedStick(AnalogValues* values, DeadZone* deadZone) {
	return (AnalogValues){ getPatchedStickValue(values->x, deadZone), getPatchedStickValue(values->y, deadZone) };	
}

void setStickValue(int *x, int *y, AnalogValues *values) {
	*x = values->x;
	*y = values->y;
}

void setLeftStick(SceCtrlData *data, AnalogValues *values) {
	setStickValue(&(data->lx), &(data->ly), values);
}

void setRigthStick(SceCtrlData *data, AnalogValues *values) {
	setStickValue(&(data->rx), &(data->ry), values);
}

void patchData(SceCtrlData *data) {
	AnalogValues left = { data->lx, data->ly };
	AnalogValues rigth = { data->rx, data->ry };

	DeadZone leftZone = { 20, 30 };
	DeadZone rigthZone = { 10, 20 };

	AnalogValues leftResult = getPatchedStick(&left, &leftZone);
	AnalogValues rigthResult = getPatchedStick(&rigth, &rigthZone);

	setLeftStick(data, &leftResult);
	setRigthStick(data, &rigthResult);
}

void loadConfig(void) {
	
}

// Simplified generic hooking functions
void hookFunctionExport(uint32_t nid, const void *func, const char *module) {
	hooks[current_hook] = taiHookFunctionExportForKernel(KERNEL_PID, &refs[current_hook], module, TAI_ANY_LIBRARY, nid, func);
	current_hook++;
}

int ksceCtrlSetSamplingMode_patched(SceCtrlPadInputMode mode) {
	if (mode == SCE_CTRL_MODE_ANALOG) mode = SCE_CTRL_MODE_ANALOG_WIDE;
	return TAI_CONTINUE(int, refs[2], mode);
}

int ksceCtrlPeekBufferPositive_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[0], port, ctrl, count);
	patchData(ctrl);
	return ret;
}

int ksceCtrlReadBufferPositive_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[1], port, ctrl, count);
	patchData(ctrl);
	return ret;
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
	
	// Setup stuffs
	loadConfig();
	
	// Hooking functions
	hookFunctionExport(0xEA1D3A34, ksceCtrlPeekBufferPositive_patched, "SceCtrl");
	hookFunctionExport(0x9B96A1AA, ksceCtrlReadBufferPositive_patched, "SceCtrl");
	
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {

	// Freeing hooks
	while (current_hook-- > 0){
		taiHookReleaseForKernel(hooks[current_hook], refs[current_hook]);
	}
		
	return SCE_KERNEL_STOP_SUCCESS;
	
}