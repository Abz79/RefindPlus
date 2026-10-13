/**
** Main/snapshots.c
**
** Copyright (c) 2026 Dayo Akanji (sf.net/u/dakanji/profile)
** Portions Copyright (c) 2026 Abdelkader Boudih
** Provided Under the GNU General Public License (v3/Later)
**/


#include "global.h"
#include "config.h"
#include "lib.h"
#include "menu.h"
#include "scan.h"
#include "mystrings.h"
#include "snapshots.h"
#include "../include/refit_call_wrapper.h"

#define DIR_SNAPSHOTS_LOC   L"@.snapshots"
#define MAX_INFO_FIELD_LEN            256
#define MAX_SNAPSHOT_DIGITS            18


static
BOOLEAN IsSnapshotId (
    IN CHAR16 *Name
) {
    UINTN      i;


    if (Name    == NULL ||
        Name[0] == L'\0'
    ) {
        return FALSE;
    }

    for (i = 0; Name[i] != L'\0'; i++) {
        if (Name[i] < L'0' || Name[i] > L'9') {
            return FALSE;
        }

        if (i >= MAX_SNAPSHOT_DIGITS) {
            return FALSE;
        }
    } // for

    if (Name[0] == L'0' && Name[1] != L'\0') {
        // Reject Leading Zeros
        return FALSE;
    }

    return TRUE;
} // static BOOLEAN IsSnapshotId()

static
UINT64 FetchIdValue (
    IN CHAR16 *Name
) {
    UINTN      i;
    UINT64     Value;


    Value = 0;
    for (i = 0; Name[i] != L'\0'; i++) {
        if (Value > (MAX_UINT64 / 10)) {
            Value = (MAX_UINT64 / 1);
            break;
        }

        Value *= 10;
        if (Value > (MAX_UINT64 - (UINT64)(Name[i] - L'0'))) {
            Value = (MAX_UINT64 - 0);
            break;
        }
        Value += (UINT64)(Name[i] - L'0');
    } // for

    return Value;
} // static UINT64 FetchIdValue()

static
BOOLEAN HasOptionToken (
    IN CHAR16 *Options,
    IN CHAR16 *Token
) {
    UINTN      i, j;
    BOOLEAN    OutFlag;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);
    if (Options == NULL || Token == NULL) {
        BREAD_CRUMB(L"%a:  A1 - END:- return 'FALSE' on early gating", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return FALSE;
    }

    i = 0;
    OutFlag = FALSE;
    while (Options[i] != L'\0') {
        while (Options[i] == L' ') i++;
        if (Options[i] == L'\0') break;

        for (j = 0; Token[j] != L'\0' && Options[i + j] == Token[j]; j++);
        if (Token[j] == L'\0' && (
            Options[i + j] == L' ' || Options[i + j] == L'\0'
        )) {
            OutFlag = TRUE;
            break;
        }

        while (Options[i] != L' ' && Options[i] != L'\0') i++;
    } // while

    BREAD_CRUMB(L"%a:  Z - END:- return OutFlag = '%s'", __func__,
        (OutFlag) ? L"TRUE" : L"FALSE"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");
    return OutFlag;
} // static BOOLEAN HasOptionToken()

static
VOID FlipRwToRo (
    IN OUT CHAR16 *Options
) {
    UINTN      i;


    if (Options == NULL) {
        return;
    }

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);
    i = 0;
    while (Options[i] != L'\0') {
        while (Options[i] == L' ') i++;
        if (Options[i] == L'\0') break;

        if (Options[i]     == L'r'  &&
            Options[i + 1] == L'w'  &&
            (
                Options[i + 2] == L' ' ||
                Options[i + 2] == L'\0'
            )
        ) {
            Options[i + 1] = L'o';
        }

        while (Options[i] != L' ' && Options[i] != L'\0') i++;
    } // while
    BREAD_CRUMB(L"%a:  Z - END");
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // static VOID FlipRwToRo()

static
BOOLEAN HasTextPrefix (
    IN CHAR16 *Text,
    IN CHAR16 *Prefix
) {
    UINTN      i;
    BOOLEAN    Flag;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);
    Flag = TRUE;
    for (i = 0; Prefix[i] != L'\0'; i++) {
        if (Text[i] != Prefix[i]) {
            Flag = FALSE;
            break;
        }
    } // for

    BREAD_CRUMB(L"%a:  Z - END:- return Flag = '%s'", __func__,
        (Flag) ? L"TRUE" : L"FALSE"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");
    return Flag;
} // static BOOLEAN HasTextPrefix()

static
CHAR16 * FetchRootFlags (
    IN CHAR16 *Options
) {
    CHAR16    *Token;
    CHAR16    *Found;
    BOOLEAN    Check;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);
    Found = NULL;
    Token = Options;
    while (*Token != L'\0') {
        while (*Token == L' ') Token++;
        if (*Token == L'\0') break;

        Check = HasTextPrefix (
            Token, L"rootflags="
        );
        if (Check) {
            Found = Token;
        }

        while (*Token != L' ' && *Token != L'\0') Token++;
    } // while

    BREAD_CRUMB(L"%a:  Z - END:- return Found = '%s'", __func__,
        (Found != NULL) ? Found : L"NULL"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");
    return Found;
} // static CHAR16 * FetchRootFlags()

static
CHAR16 * SetSnapshotOptions (
    IN CHAR16 *BaseOptions OPTIONAL,
    IN CHAR16 *SubVolPath
) {
    BOOLEAN    Check;
    CHAR16    *Cur;
    CHAR16    *Elem;
    CHAR16    *Stop;
    CHAR16    *Tail;
    CHAR16    *Match;
    CHAR16    *Prefix;
    CHAR16    *Others;
    CHAR16    *Result;
    CHAR16    *TmpStr;
    CHAR16    *Value;
    UINTN      Index;
    UINTN      ValueLen;
    UINTN      PrefixLen;


    if (SubVolPath == NULL) {
        return NULL;
    }

    if (BaseOptions == NULL) {
        return PoolPrint (
            L"ro rootflags=subvol=%s",
            SubVolPath
        );
    }

    Match = FetchRootFlags (
        BaseOptions
    );
    if (Match == NULL) {
        Result = PoolPrint (
            L"%s rootflags=subvol=%s",
            BaseOptions, SubVolPath
        );
    }
    else {
        // Split into Prefix + 'rootflags=' + Value + Tail
        Value = Match + 10;        // L"rootflags=" Length
        Tail  = Value;
        while (*Tail != L' ' && *Tail != L'\0') {
            Tail++;
        } // while

        ValueLen  = (UINTN)(Tail  - Value);
        PrefixLen = (UINTN)(Match - BaseOptions);

        Prefix = AllocateZeroPool (
            (PrefixLen + 1) * sizeof (CHAR16)
        );
        if (Prefix == NULL) return NULL;

        Others = AllocateZeroPool (
            (ValueLen + 2) * sizeof (CHAR16)
        );
        if (Others == NULL) {
            MY_FREE_POOL(Prefix);
            return NULL;
        }

        REFIT_CALL_3_WRAPPER(
            gBS->CopyMem, Prefix,
            BaseOptions, PrefixLen * sizeof(CHAR16)
        );

        Index = 0;
        Elem  = Value;
        while (Elem < Tail) {
            Stop = Elem;
            while (Stop < Tail && *Stop != L',') {
                Stop++;
            } // while

            if (Stop > Elem                        &&
                !HasTextPrefix (Elem, L"subvol=")  &&
                !HasTextPrefix (Elem, L"subvolid=")
            ) {
                Others[Index++] = L',';
                for (Cur = Elem; Cur < Stop; Cur++) {
                    Others[Index++] = *Cur;
                } // for
            }

            if (Stop >= Tail) break;
            Elem = Stop + 1;
        } // while

        // NB: No space between 'Prefix' and 'rootflags'
        //     as 'Prefix' should have trailing space
        Result = PoolPrint (
            L"%srootflags=subvol=%s%s%s",
            Prefix, SubVolPath, Others, Tail
        );

        MY_FREE_POOL(Prefix);
        MY_FREE_POOL(Others);
    } // if/else Match == NULL
    if (Result == NULL) return NULL;

    Check = HasOptionToken (Result, L"ro");
    if (Check) return Result;

    Check = HasOptionToken (Result, L"rw");
    if (Check) {
        FlipRwToRo (Result);
    }
    else {
        TmpStr = Result;
        Result = PoolPrint (L"ro %s", TmpStr);
        if (Result == NULL) {
            Result = TmpStr;
        }
        else {
            MY_FREE_POOL(TmpStr);
        }
    }

    return Result;
} // static CHAR16 * SetSnapshotOptions()

static
CHAR16 * ParseInfo (
    IN CHAR8 *Text,
    IN UINTN  Size,
    IN CHAR8 *Field
) {
    CHAR16   *Tag;
    UINTN     i, j;
    UINTN     DataLen;
    UINTN     Start;
    UINTN     End;
    UINTN     Length;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    if (Text == NULL || Field == NULL || Size == 0) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- return 'NULL' early", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return NULL;
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    for (DataLen = 0; Field[DataLen] != '\0'; DataLen++) {}

    BREAD_CRUMB(L"%a:  3", __func__);
    if (Size < ((2 * DataLen) + 5)) {
        BREAD_CRUMB(L"%a:  3a 1 - END:- return 'NULL' early", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return NULL;
    }

    BREAD_CRUMB(L"%a:  4", __func__);
    Tag = NULL;
    for (i = 0; i < (Size - DataLen - 1); i++) {
        if (Text[i] != '<') continue;

        for (j = 0; j < DataLen && Text[i + 1 + j] == Field[j]; j++);
        if (j < DataLen || Text[i + 1 + DataLen] != '>') continue;

        Start = i + DataLen + 2;
        for (End = Start; End < Size && Text[End] != '<'; End++);
        if (End <= Start || End >= Size) {
            BREAD_CRUMB(L"%a:  4a 1 - Break Loop", __func__);
            break;
        }

        // Truncate for sanity
        Length = End - Start;
        if (Length > MAX_INFO_FIELD_LEN) {
            Length = MAX_INFO_FIELD_LEN;
        }

        Tag = MyAsciiStrCopyToUnicode (
            &Text[Start], Length
        );
        BREAD_CRUMB(L"%a:  4a 2 - Break Loop", __func__);
        break;
    } // for

    BREAD_CRUMB(L"%a:  5 - END:- return Tag = '%s'", __func__,
        (Tag != NULL) ? Tag : L"NULL"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");
    return Tag;
} // static CHAR16 * ParseInfo()

static
CHAR16 * SetSnapshotTitle (
    IN REFIT_VOLUME *Volume,
    IN UINT64        Id
) {
    EFI_STATUS       Status;
    UINTN            Length;
    CHAR16          *Title;
    CHAR16          *Base;
    CHAR16          *Path;
    CHAR16          *Date;
    CHAR16          *Desc;
    REFIT_FILE      *Info;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);
    Info = NULL;
    Date = NULL;
    Desc = NULL;
    Path = PoolPrint (
        L"%s\\%Ld\\info.xml",
        DIR_SNAPSHOTS_LOC, Id
    );
    if (Path != NULL) {
        Info = AllocateZeroPool (
            sizeof (REFIT_FILE)
        );
        if (Info == NULL) {
            MY_FREE_POOL(Path);

            BREAD_CRUMB(L"%a:  A1 - END:- return NULL on allocation failure", __func__);
            LOG_DECREMENT();
            LOG_SEP(L"X");
            return NULL;
        }

        Status = RefitReadFile (
            Volume->RootDir, Path,
            Info, &Length
        );
        if (!EFI_ERROR (Status)      &&
            Info->BufferSize > 0     &&
            Info->BufferData != NULL
        ) {
            Date = ParseInfo (
                (CHAR8 *) Info->BufferData,
                Info->BufferSize, "date"
            );
            Desc = ParseInfo (
                (CHAR8 *) Info->BufferData,
                Info->BufferSize, "description"
            );
        }

        MY_FREE_FILE(Info);
        MY_FREE_POOL(Path);
    } // if Path != NULL

    Base = PoolPrint (
        L"Load Instance: BtrFS Snapshot[%04Ld]", Id
    );
    if (Base == NULL) {
        Title = NULL;
    }
    else if (Date != NULL && Desc != NULL) {
        Title = PoolPrint (
            L"%s ::: %s (%s)",
            Base, Date, Desc
        );
    }
    else if (Date != NULL || Desc != NULL) {
        Title = PoolPrint (
            L"%s ::: %s", Base,
            (Date != NULL) ? Date : Desc
        );
    }
    else {
        Title = StrDuplicate (Base);
    }

    MY_FREE_POOL(Base);
    MY_FREE_POOL(Date);
    MY_FREE_POOL(Desc);

    BREAD_CRUMB(L"%a:  Z - END:- return Title = '%s'", __func__,
        (Title != NULL) ? Title : L"NULL"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");
    return Title;
} // static CHAR16 * SetSnapshotTitle()

static
UINTN FetchSnapshotIds (
    IN  REFIT_VOLUME *Volume,
    OUT UINT64       *TheIds
) {
    UINTN             i, j;
    UINTN             Count;
    UINT64            OurID;
    CHAR16           *Probe;
    BOOLEAN           Check;
    BOOLEAN           NotDup;
    EFI_FILE_INFO    *DirEntry;
    REFIT_DIR_ITER    DirIter;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);
    DirIterOpen (
        Volume->RootDir,
        DIR_SNAPSHOTS_LOC,
        &DirIter
    );

    Count = 0;
    while (1) {
        Check = DirIterNext (
            &DirIter, 1,
            NULL, &DirEntry
        );
        if (!Check) break;

        Check = IsSnapshotId (
            DirEntry->FileName
        );
        if (!Check) {
            MY_FREE_POOL(DirEntry);
            continue;
        }

        // NB: 'TheIds[]' is newest to oldest
        //      Based on numeric value though
        OurID = FetchIdValue (
            DirEntry->FileName
        );
        for (i = 0; i < Count; i++) {
            if (OurID >= TheIds[i]) {
                break;
            }
        } // for

        NotDup = (
            i >= Count || OurID != TheIds[i]
        ) ? TRUE : FALSE;
        if (i < GlobalConfig.MaxSnapshots && NotDup) {
            Probe = PoolPrint (
                L"%s\\%s\\snapshot",
                DIR_SNAPSHOTS_LOC,
                DirEntry->FileName
            );
            if (Probe == NULL) {
                MY_FREE_POOL(DirEntry);
                continue;
            }

            Check = FileExists (
                Volume->RootDir, Probe
            );
            MY_FREE_POOL(Probe);
            if (!Check) {
                MY_FREE_POOL(DirEntry);
                continue;
            }

            for (j = (
                    Count < (GlobalConfig.MaxSnapshots - 0)
                ) ? Count : (GlobalConfig.MaxSnapshots - 1);
                j > i; j--
            ) {
                TheIds[j] = TheIds[j - 1];
            } // for
            TheIds[i] = OurID;

            if (Count < GlobalConfig.MaxSnapshots) {
                Count++;
            }
        }

        MY_FREE_POOL(DirEntry);
    } // while {Infinite}

    DirIterClose (&DirIter);

    BREAD_CRUMB(L"%a:  Z - END:- return Count = '%Ld'", __func__,
        (UINT64) Count
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");
    return Count;
} // static UINTN FetchSnapshotIds()

VOID AddBaseSnapshots (
    IN     LOADER_ENTRY      *Entry,
    IN OUT REFIT_MENU_SCREEN *SubScreen,
    IN     CHAR16            *BootOptions  OPTIONAL
) {
    UINTN                     i;
    UINTN                     Count;
    CHAR16                   *Title;
    CHAR16                   *Subvol;
    CHAR16                   *Options;
    UINT64                    Ids[MAX_SNAPSHOTS];
    LOADER_ENTRY             *SubEntry;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    if (GlobalConfig.MaxSnapshots < 1) {
        BREAD_CRUMB(L"%a:  a 1 - END:- Gating on MaxSnapshots", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return;
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    if (Entry                  == NULL ||
        Entry->Volume          == NULL ||
        Entry->Volume->RootDir == NULL ||
        Entry->Volume->FSType  != FS_TYPE_BTRFS
    ) {
        BREAD_CRUMB(L"%a:  2a 1 - END:- Gating on Entry", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return;
    }

    BREAD_CRUMB(L"%a:  3", __func__);
    if (SubScreen == NULL) {
        BREAD_CRUMB(L"%a:  3a 1 - END:- Gating on SubScreen", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return;
    }

    BREAD_CRUMB(L"%a:  4", __func__);
    Count = FetchSnapshotIds (
        Entry->Volume, Ids
    );
    BREAD_CRUMB(L"%a:  5", __func__);
    if (Count == 0) {
        BREAD_CRUMB(L"%a:  6a 1 - END:- Gating on Count", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return;
    }

    BREAD_CRUMB(L"%a:  6", __func__);
    for (i = 0; i < Count; i++) {
        LOG_SEP(L"X");
        BREAD_CRUMB(L"%a:  6a 1 - FOR LOOP:- START", __func__);
        Subvol = PoolPrint (
            L"@.snapshots/%Ld/snapshot", Ids[i]
        );
        BREAD_CRUMB(L"%a:  6a 2", __func__);
        if (Subvol == NULL) {
            BREAD_CRUMB(L"%a:  6a 2a 1 - FOR LOOP:- SKIP ... Subvol == NULL", __func__);
            LOG_SEP(L"X");
            continue;
        }

        BREAD_CRUMB(L"%a:  6a 3", __func__);
        Options = SetSnapshotOptions (
            BootOptions, Subvol
        );
        BREAD_CRUMB(L"%a:  6a 4", __func__);
        MY_FREE_POOL(Subvol);
        if (Options == NULL) {
            BREAD_CRUMB(L"%a:  6a 4a 1 - FOR LOOP:- SKIP ... Options == NULL", __func__);
            LOG_SEP(L"X");
            continue;
        }

        BREAD_CRUMB(L"%a:  6a 5", __func__);
        Title = SetSnapshotTitle (
            Entry->Volume, Ids[i]
        );
        BREAD_CRUMB(L"%a:  6a 6", __func__);
        if (Title == NULL) {
            MY_FREE_POOL(Options);

            BREAD_CRUMB(L"%a:  6a 6a 1 - FOR LOOP:- SKIP ... Title == NULL", __func__);
            LOG_SEP(L"X");
            continue;
        }

        BREAD_CRUMB(L"%a:  6a 7", __func__);
        SubEntry = CopyLoaderEntry (Entry);

        BREAD_CRUMB(L"%a:  6a 8", __func__);
        if (SubEntry == NULL) {
            MY_FREE_POOL(Options);
            MY_FREE_POOL(Title);

            BREAD_CRUMB(L"%a:  6a 8a 1 - FOR LOOP:- SKIP ... SubEntry == NULL", __func__);
            LOG_SEP(L"X");
            continue;
        }

        BREAD_CRUMB(L"%a:  6a 9", __func__);
        // NB: Free here is actually redundant
        //     as not set in CopyLoaderEntry.
        //     Keeping in case this changes.
        MY_FREE_POOL(SubEntry->me.Title);
        SubEntry->me.Title = Title;

        BREAD_CRUMB(L"%a:  6a 10", __func__);
        MY_FREE_POOL(SubEntry->LoadOptions);
        SubEntry->LoadOptions = Options;

        BREAD_CRUMB(L"%a:  6a 11", __func__);
        SubEntry->UseGraphicsMode = (
            GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX
        );

        BREAD_CRUMB(L"%a:  6a 12", __func__);
        AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);

        BREAD_CRUMB(L"%a:  6a 13 - FOR LOOP:- END", __func__);
        LOG_SEP(L"X");
    } // for

    BREAD_CRUMB(L"%a:  6 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID AddBaseSnapshots()
