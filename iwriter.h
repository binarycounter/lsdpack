/* lsdpack - standalone LSDj (Little Sound Dj) recorder + player {{{
   Copyright (C) 2021  Johan Kotlinski
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

#pragma once

class IWriter {
    public:
        virtual ~IWriter() {}

        virtual void record_song_start(const char* out_path) = 0;
        virtual void record_song_stop() = 0;
        virtual void record_write(unsigned char addr, unsigned char data, unsigned long cycle) = 0;
        virtual void record_end_tick() = 0;
        virtual void write_music_to_disk() = 0;
};
