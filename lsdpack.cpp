/* lsdpack - standalone LSDj (Little Sound Dj) recorder + player {{{
   Copyright (C) 2018  Johan Kotlinski
   https://www.littlesounddj.com

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. }}} */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include "gambatte.h"

#include "input.h"
#include "writer.h"
#include "dumpwriter.h"
#include "getopt.h"

gambatte::GB gameboy;
Input input;
std::string out_path;

IWriter* writer;

void run_one_frame() {
    size_t samples = 35112;
    static gambatte::uint_least32_t audioBuffer[35112 + 2064];
    gameboy.runFor(0, 0, &audioBuffer[0], samples);
}

void wait(float seconds) {
    for (float i = 0.f; i < 60.f * seconds; ++i) {
        run_one_frame();
    }
}

void press(unsigned key, float seconds = 0.1f) {
    input.press(key);
    if (!gameboy.isCgb() && seconds > 0.1f) {
        seconds *= 2; // compensates for slow CPU
    }
    wait(seconds);
}

bool load_song(int position) {
    press(SELECT);
    press(SELECT | UP);
    press(0);
    for (int i = 0; i < 16; ++i) {
        press(DOWN); // go to bottom
        press(0);
    }
    press(A);
    press(0);
    press(A);
    press(0);
    for (int i = 0; i < 32; ++i) {
        press(UP); // go to top
        press(0);
    }
    for (int i = 0; i < position; ++i) {
        press(DOWN);
        press(0);
    }
    // press song name
    press(A);
    press(0);
    // discard changes
    press(LEFT);
    press(0);
    press(A);
    // wait until song is loaded
    press(0, 5);
    if (gameboy.isSongEmpty()) {
        return false;
    }
    return true;
}

bool sound_enabled;

void play_song() {
    int seconds_elapsed = 0;
    sound_enabled = false;
    writer->record_song_start(out_path.c_str());
    press(START);
    if (!sound_enabled) {
        fputs("Aborted: Song did not start.\n", stderr);
        exit(1);
    }
    while (sound_enabled) {
        wait(1);

        if (++seconds_elapsed == 60 * 60) {
            fputs("Aborted: Song still playing after one hour. Please add a HFF command to song end to stop recording.\n", stderr);
            exit(1);
        }
    }
}

void on_ff_write(char p, char data, unsigned long cycle) {
    if (p < 0x10 || p >= 0x40) {
        return; // not sound
    }
    switch (p) {
        case 0x26:
            if (sound_enabled && !data) {
                writer->record_song_stop();
                sound_enabled = false;
                return;
            }
            sound_enabled = data;
            break;
    }
    if (sound_enabled) {
        writer->record_write(p, data, cycle);
    }
}

void on_tick_interrupt() {
    if (sound_enabled) {
        writer->record_end_tick();
    }
}

void make_out_path(const char* in_path, std::string suffix) {
    if (strlen(in_path) < strlen("a.gb")) {
        return;
    }

    out_path = in_path;
    out_path.replace(out_path.end() - 3, out_path.end(), ""); // strip .gb
    out_path += suffix;
    out_path.replace(out_path.begin(), out_path.begin() + 1 + out_path.rfind('/'), "");
    out_path.replace(out_path.begin(), out_path.begin() + 1 + out_path.rfind('\\'), "");
    printf("Recording to '%s'\n", out_path.c_str());
}

enum LSDJCompatibility { v9_2_0, v9_1_0, auto_detect };
void load_gb(const char* path, bool dmg_mode, LSDJCompatibility compatibility) {
    unsigned flags = dmg_mode ? gameboy.FORCE_DMG : 0;
    if (gameboy.load(path, flags)) {
        fprintf(stderr, "Loading %s failed\n", path);
        exit(1);
    }

    if (compatibility == auto_detect)
    {
        puts("Auto-detecting LSDj version");
        std::ifstream rom_file(path);
        char header_string[0xF];
        rom_file.seekg(0x134);
        rom_file.read(header_string, 0xf);
        rom_file.close();
        const char* start = strstr(header_string, "v");
        if (start == nullptr) {
            fprintf(stderr, "Could not detect LSDj version for %s \n Please specify version using command-line options\n", path);
            exit(1);
        }
        if (start[1] >= '9' && start[3] >= '2')
            compatibility = v9_2_0;
        else
            compatibility = v9_1_0;
    }

    if (compatibility == v9_2_0)
    {
        puts("9.2.0+ mode, recording timer interrupts");
        gameboy.setTimerHandler(on_tick_interrupt);
    }
    else
    {
        puts("<9.1.C mode, recording LCD interrupts");
        gameboy.setLcdHandler(on_tick_interrupt);
    }

    printf("Loaded %s\n", path);
    press(0, 3);

    /* Press B to skip the first LittleFM screen, if the ROM is patched with it */
    press(B);
    press(0);
}

void record_gb(int argc, char* argv[], bool dmg_mode, LSDJCompatibility compatibility) {
    make_out_path(argv[optind], ".s");
    writer = new Writer(false);
    unsigned flags = dmg_mode ? gameboy.FORCE_DMG : 0;

    for (; optind < argc; ++optind) {
        load_gb(argv[optind], flags, compatibility);

        for (int song_index = 0; song_index < 32; ++song_index) {
            if (load_song(song_index)) {
                printf("Playing song %i...\n", song_index + 1);
                play_song();
            }
        }
    }
    writer->write_music_to_disk();

    delete writer;
}

void record_gbs(int argc, char* argv[], bool dmg_mode, LSDJCompatibility compatibility) {
    unsigned flags = dmg_mode ? gameboy.FORCE_DMG : 0;
    for (; optind < argc; ++optind) {
        load_gb(argv[optind], flags, compatibility);

        for (int song_index = 0; song_index < 32; ++song_index) {
            writer = new Writer(true);

            if (load_song(song_index)) {
                printf("Playing song %i...\n", song_index + 1);

                char suffix[20];
                snprintf(suffix, sizeof(suffix), "-%i.s", song_index + 1);
                make_out_path(argv[optind], suffix);

                play_song();
                writer->write_music_to_disk();
            }

            delete writer;
        }
    }
}

void record_dump(int argc, char* argv[], bool dmg_mode, LSDJCompatibility compatibility) {
    writer = new DumpWriter();
    unsigned flags = dmg_mode ? gameboy.FORCE_DMG : 0;

    for (; optind < argc; ++optind) {
        load_gb(argv[optind], flags, compatibility);

        for (int song_index = 0; song_index < 32; ++song_index) {
            if (load_song(song_index)) {
                printf("Playing song %i...\n", song_index + 1);

                char suffix[20];
                snprintf(suffix, sizeof(suffix), "-%i.txt", song_index + 1);
                make_out_path(argv[optind], suffix);

                play_song();
            }
        }
    }
    delete writer;
}

void print_help_and_exit() {
    puts("usage: lsdpack [-g] [-r] [-d] [-t/l] [lsdj.gb lsdj2.gb ...]");
    puts("");
    puts("-g: .gbs conversion");
    puts("-r: raw register dump; optimizations disabled");
    puts("-d: record using emulated DMG (default: CGB)");
    puts("-t: force 9.2.0+ compatibility mode (default: auto-detect)");
    puts("-l: force <9.1.0 compatibility mode (default: auto-detect)");
    exit(1);
}

int main(int argc, char* argv[]) {
    bool gbs_mode = false;
    bool dump_mode = false;
    bool dmg_mode = false;
    LSDJCompatibility compatibility=auto_detect;
    unsigned flags = 0;

    int c;
    while ((c = getopt(argc, argv, "grdtl")) != -1) {
        switch (c) {
            case 'g':
                puts(".gbs mode enabled");
                gbs_mode = true;
                break;
            case 'r':
                puts("register dump mode enabled");
                dump_mode = true;
                break;
            case 'd':
                puts("recording using emulated DMG");
                dmg_mode = true;
                break;
            case 't':
                 compatibility=v9_2_0;
                break;
            case 'l':
                compatibility = v9_1_0;
                break;
            default:
                print_help_and_exit();
        }
    }

    if (argc <= optind) {
        print_help_and_exit();
    }

    gameboy.setInputGetter(&input);
    gameboy.setWriteHandler(on_ff_write);


    
    if (dmg_mode) {
        flags |= gameboy.FORCE_DMG;
    }
    if (gbs_mode) {
        record_gbs(argc, argv, flags, compatibility);
    } else if (dump_mode) {
        record_dump(argc, argv, flags, compatibility);
    } else {
        record_gb(argc, argv, flags, compatibility);
    }

    puts("OK");
}
