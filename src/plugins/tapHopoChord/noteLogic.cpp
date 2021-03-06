#include "core\Patcher.h"
#include "gh3\GH3Keys.h"
#include "gh3\GH3GlobalAddresses.h"
#include "tapHopoChord.h"


FretMask getHighFret(FretMask fretMask)
{
	if (fretMask & FretMask::ORANGE)
		return  FretMask::ORANGE;

	if (fretMask & FretMask::BLUE)
		return  FretMask::BLUE;

	if (fretMask & FretMask::YELLOW)
		return  FretMask::YELLOW;

	if (fretMask & FretMask::RED)
		return  FretMask::RED;

	if (fretMask & FretMask::GREEN)
		return  FretMask::GREEN;

	return FretMask::OPEN;
}






//Types


//Detour addresses

static void * const guitarInputLogicDetour1 = (void *)0x00431A79;
static void * const guitarInputLogicDetour2 = (void *)0x00431B35;
static void * const guitarInputLogicDetour2b = (void *)0x00431906;
static void * const chordHitCheckDetour = (void *)0x00431EA6;
static void * const noteHitEndDetour = (void *)0x004320AD;
static void * const noteHitEndDetour2 = (void *)0x0043214C;

//GH3 globals
static uint32_t *g_hitFrets = (uint32_t *)ADDR_hitFrets;
static uint32_t **g_crazyPointerChain = (uint32_t **)ADDR_CrazyPointerChain;
static uint32_t *g_currentNote = (uint32_t *)ADDR_currentNote;
static uint32_t *g_pressedFrets = (uint32_t *)ADDR_pressedFrets;
static uint32_t *g_hitNoNotes_bool = (uint32_t *)ADDR_hitNoNotes_bool;

//Hack globals
static uint32_t g_canTap[2] = { 0 };
static uint32_t g_canTapLow[2] = { 0 };
static uint32_t g_lastHit[2] = { 0 };

//Pre-declarations
uint32_t *LoadCurrentNoteStruct(uint32_t pIdx);
uint32_t getAnchoredMask(uint32_t fretMask);
bool hitHopoChordHigh(uint32_t fretsHeld, uint32_t noteMask);
bool hitHopoChordLow(uint32_t fretsHeld, uint32_t noteMask);
uint32_t __stdcall loadLastHit(uint32_t pIdx);
int __stdcall chordHitCheck(HopoFlag hopoFlag, uint32_t fretsHeld, uint32_t noteMask, uint32_t pIdx);
void __stdcall noteHitEnd(uint32_t pIdx);




////////////////
// ~~~~~~~~~~ //
////////////////







////////////////
// ~~~~~~~~~~ //
////////////////

////////////////
// ~~~~~~~~~~ //
////////////////

////////////////
// ~~~~~~~~~~ //
////////////////



__declspec(naked) void loadLastHitNaked()
{
	static const uint32_t returnAddress = 0x00431A80;

	_asm
	{
		PUSH_ALL_BUT_EDX

		push ecx; //pIdx
		call loadLastHit;
		mov edx, eax;

		POP_ALL_BUT_EDX

			jmp returnAddress;
	};
}

uint32_t __stdcall loadLastHit(uint32_t pIdx)
{
	HopoFlag hopoFlag;

	uint32_t *noteStruct = LoadCurrentNoteStruct(pIdx);
	uint32_t fretCount = 0;
	bool isChord = false;

	//Get the hopo flag out of the note structure
	if (noteStruct[1] == 1)
	{
		//1D Case (this probably doesn't ever get hit because it seems like insanity)
		HopoFlag *flagPtr = reinterpret_cast<HopoFlag*>(noteStruct);
		hopoFlag = flagPtr[2];
	}
	else
	{
		//2D Case
		Note &note = *reinterpret_cast<Note *>(noteStruct[2]);
		hopoFlag = note.hopoFlag;
		for (int i = 0; i < 5; ++i)
		{
			if (note.fret[i] == 1)
				++fretCount;
		}
		if (fretCount > 1)
			isChord = true;
	}

	//Accounting for chords here fixes an edge case that would be painful to fix in later code
	if (hopoFlag == HopoFlag::HOPO && g_canTapLow[pIdx] && g_hitFrets[pIdx] != 0x00000 && isChord) //tapping note
	{
		return 0x11111111; //Impossible fretmask, guarantees that the game will think we're on a streak and can therefore hammer the next note
	}

	if (hopoFlag == HopoFlag::TAPPING && (g_canTap[pIdx] || (g_canTapLow[pIdx] && isChord))) //tapping note
	{
		return 0x11111111; //Impossible fretmask, guarantees that the game will think we're on a streak and can therefore hammer the next note
	}
	else
	{
		return g_hitFrets[pIdx];
	}

}



uint32_t *LoadCurrentNoteStruct(uint32_t pIdx)
{

	uint32_t *playerChain = g_crazyPointerChain[pIdx];
	if (playerChain[1] == 1)
	{
		uint32_t **playerAltChain = reinterpret_cast<uint32_t **>(playerChain);
		return playerAltChain[2];
	}
	else
	{
		uint32_t ***playerAltChain = reinterpret_cast<uint32_t ***>(playerChain);
		return playerAltChain[2][g_currentNote[pIdx]];
	}
}



////////////////
// ~~~~~~~~~~ //
////////////////

////////////////
// ~~~~~~~~~~ //
////////////////

////////////////
// ~~~~~~~~~~ //
////////////////

void __stdcall setCanTapState(FretMask currFrets, FretMask prevFrets, uint32_t pIdx)
{
	if (getHighFret(currFrets) != getHighFret(prevFrets))
		g_canTap[pIdx] = true;

	if (currFrets != prevFrets)
		g_canTapLow[pIdx] = true;
}

__declspec(naked) void setCanTapStateNaked()
{
	static const uint32_t returnAddress = 0x00431B43;

	_asm
	{
		pushad;
		push ecx;                         //arg 3 pIdx
		push ebx;                         //arg 2 currFrets
		mov eax, [ADDR_pressedFrets + ecx * 4];
		push eax;                         //arg 1 prevFrets
		call setCanTapState;
		popad;

		mov eax, [ADDR_CrazyPointerChain + ecx * 4];
		mov[ADDR_pressedFrets + ecx * 4], ebx;
		jmp returnAddress;
	};
}





////////////////
// ~~~~~~~~~~ //
////////////////



__declspec(naked) void chordHitCheckNaked()
{

	static const uint32_t CHORD_CHECK_FAILED = 0x00431EE3;
	static const uint32_t CHORD_CHECK_PASSED = 0x00431EBA;
	static const uint32_t NOTE_HIT_HOPO = 0x00431F50;

	static const int frameOffset = 0x198;

	static const int isHopo2 = -0x164 + frameOffset; //0x34
	static const int pIdx = -0x170 + frameOffset; //0x28
	static const int pIdx9 = -0x188 + frameOffset; //0x10

	__asm
	{
		mov		ecx, [esp + 0x10];
		push	ecx;                  // pIdx
		push	ebx;                    // note mask
		push	[ADDR_pressedFrets + ecx * 4] // pressed frets
		push	[esp + 0x40];        // hopo flag
		call	chordHitCheck;
		cmp		eax, 1;
		je		L_NOTE_HIT_HOPO;

		//Normal chord. Not HOPO (or hopo failed)
		cmp		byte ptr[esp + 17h], 0;  //var_184 + 3. I don't know what this is
		jnz		L_CHORD_CHECK_PASSED;

		mov		ecx, [esp + 0x10]; //pIdx9 
		cmp		[ADDR_hitNoNotes_bool + ecx * 4], edi;
		jnz		L_CHORD_CHECK_FAILED;

		jmp		CHORD_CHECK_PASSED;

	L_CHORD_CHECK_FAILED :
		jmp		CHORD_CHECK_FAILED;
	L_CHORD_CHECK_PASSED:
		jmp		CHORD_CHECK_PASSED;
	L_NOTE_HIT_HOPO:
		jmp		NOTE_HIT_HOPO;
	}
}

int __stdcall chordHitCheck(HopoFlag hopoFlag, uint32_t fretsHeld, uint32_t noteMask, uint32_t pIdx)
{
	if (hopoFlag == HopoFlag::STRUMMED)
		return 0;

	if (g_canTap[pIdx] && hitHopoChordHigh(fretsHeld, noteMask))
		return 1;

	if (g_canTapLow[pIdx] && hitHopoChordLow(fretsHeld, noteMask))
		return 1;

	return 0;
}

bool hitHopoChordHigh(uint32_t fretsHeld, uint32_t noteMask)
{
	if ((fretsHeld & noteMask) != noteMask)
		return false;

	if ((fretsHeld & getAnchoredMask(noteMask)) != fretsHeld)
		return false;

	return true;
}

bool hitHopoChordLow(uint32_t fretsHeld, uint32_t noteMask)
{
	return (fretsHeld == noteMask);
}


uint32_t getAnchoredMask(uint32_t fretMask)
{
	if (fretMask >= FretMask::GREEN)
		return fretMask;

	if (fretMask >= FretMask::RED)
		return fretMask | 0x10000;

	if (fretMask >= FretMask::YELLOW)
		return fretMask | 0x11000;

	if (fretMask >= FretMask::BLUE)
		return fretMask | 0x11100;

	if (fretMask >= FretMask::ORANGE)
		return fretMask | 0x11110;

	return 0x00000;
	
	//return 0x11111;
}



////////////////
// ~~~~~~~~~~ //
////////////////

////////////////
// ~~~~~~~~~~ //
////////////////

////////////////
// ~~~~~~~~~~ //
////////////////



__declspec(naked) void noteHitEndNaked()
{
	const static uint32_t normalReturnAddress = 0x00432166;
	const static uint32_t debugReturnAddress = 0x00432153;

	__asm
	{
		pushad;
		mov eax, [esp + 30h]; //pIdx9 + 20h
		push eax;
		call noteHitEnd;
		popad;

		//original displaced code
		cmp dword ptr[esp + 2Ch], 0; //198h + input.field_0
		jz L_NOT_DEBUG;
		jmp debugReturnAddress;
	L_NOT_DEBUG:
		jmp normalReturnAddress;
	}
}

void __stdcall noteHitEnd(uint32_t pIdx)
{
	g_lastHit[pIdx] = g_pressedFrets[pIdx];
	g_canTap[pIdx] = 0;
	g_canTapLow[pIdx] = 0;
}

bool TryApplyNoteLogicPatches()
{
	uint8_t *jnz_to_CHORD_CHECK_FAILED = (uint8_t *)(0x00431E75); // test before was cmp hopo vs 1

	return (g_patcher.WriteJmp(guitarInputLogicDetour1, &loadLastHitNaked) &&
			g_patcher.WriteJmp(guitarInputLogicDetour2, &setCanTapStateNaked) &&
			g_patcher.WriteJmp(chordHitCheckDetour, &chordHitCheckNaked) &&
			g_patcher.WriteInt8(jnz_to_CHORD_CHECK_FAILED, 0x7C) &&
			g_patcher.WriteJmp(noteHitEndDetour2, &noteHitEndNaked));
}