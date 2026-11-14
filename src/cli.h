#pragma once

#include <QStringList>

// The commands that need no display, for scripts and coding agents.
namespace Cli {
// True when the arguments (without the program name) name a command here
// rather than asking for the window.
bool handles(const QStringList &arguments);
int run(const QStringList &arguments);
}
