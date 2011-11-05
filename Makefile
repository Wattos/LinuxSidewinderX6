#    Makefile for sidewinder-x6-macro-keys
#    Copyright (C) 2011 Filip Wieladek
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

#uncomment to enable debugging output
#DEBUG_FLAG= -DDEBUG

CC        = gcc 
CXXFLAGS  = -O3 -g -Wall -Wwrite-strings $(DEBUG_FLAG) -Wno-unused-result
LDFLAGS   = -lusb-1.0 
INCLUDES  =

HDRS      = 
SRCS      = sidewinder.c
OBJS      = $(SRCS:.c=.o)
EXEC      = sidewinder-x6-macro-keys

all : $(EXEC)

$(EXEC) : $(OBJS)
	$(CC) $(INCLUDES) $(CXXFLAGS) $(OBJS) $(LDFLAGS) -o $(EXEC) 

clean   :
	rm -f *.o $(EXEC)

.c.o:
	$(CC) $(INCLUDES) $(CXXFLAGS) -c $<  -o $@

depend  : $(HDRS) $(SRCS)
	makedepend -- $(CXXFLAGS) -- $(SRCS)
