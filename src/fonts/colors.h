//***********************************************************
//
//  File:     colors.h
//
//  Author:   Matthew Beldyk
//  Email:    mb245002@ohiou.edu
//
//  Usage:    I created this file to house some named string
//            constants with escape codes for colors in them
//            this makes it much easier for me to do colors.
//            I can still use the codes if I want, but this
//            works too.  try the statement:
//            cout<<BLUE<<"I like cookies"<<endl;
//
//		  You may use this whereever you want, but if you
//		  make any large improvements or whatever, I am
//		  curious, so email 'em my way, please.
//
//***********************************************************
//
//  all credit given to Matthew Beldyk for writing this file
//  he gave me permission to try out in my programs
//  just wanted to use to make everything look nice
//
//***********************************************************

#ifndef COLORS_H
#define COLORS_H

#include <string>

//don't use blink your
//professor will probably
//beat you to death
const std::string BLINK     = "\e[5m";

const std::string BOLD      = "\e[1m";

const std::string RESET     = "\e[0m";
const std::string ERROR     = "\e[1;41;37m\a";
const std::string MENU       = "\e[44;37m";

const std::string T_BLACK      = "\e[30m";
const std::string T_RED        = "\e[31m";
const std::string T_GREEN      = "\e[32m";
const std::string T_YELLOW     = "\e[33m";
const std::string T_BLUE       = "\e[34m";
const std::string T_MAGENTA    = "\e[35m";
const std::string T_CYAN       = "\e[36m";
const std::string T_WHITE      = "\e[37m";

const std::string B_BLACK    = "\e[40m";
const std::string B_RED      = "\e[41m";
const std::string B_GREEN   = "\e[42m";
const std::string B_YELLOW  = "\e[43m";
const std::string B_BLUE    = "\e[44m";
const std::string B_MAGENTA = "\e[45m";
const std::string B_CYAN    = "\e[46m";
const std::string B_WHITE   = "\e[47m";

#endif //COLORS_H
