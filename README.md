<p align="center">
  <picture>
    <img style="max-width:400px;height:auto"  src="https://www.rewindtty.dev/assets/images/logo-black.png" alt="rewindtty logo">
  </picture>
</p>

A terminal session recorder and replayer written in C that allows you to capture and replay terminal sessions with precise timing.

<p align="center">
  <picture>
    <img src="https://rewindtty.dev/assets/images/demo.gif" alt="rewindtty demo">
  </picture>
</p>

## ⚠️ Disclaimer

This is a hobby project created for fun and learning purposes. It's still in active development and may contain bugs or incomplete features. If you encounter any issues, please report them in the [Issues](https://github.com/debba/rewindtty/issues) section. Contributions are welcome and encouraged!

## Features

- **Record terminal sessions**: Capture all terminal input/output with accurate timing information
- **Replay sessions**: Play back recorded sessions with original timing
- **Session analysis**: Analyze recorded sessions with detailed statistics and insights
- **JSON format**: Sessions are stored in a structured JSON format for easy parsing
- **Signal handling**: Graceful shutdown and file closure on interruption
- **Lightweight**: Minimal dependencies, written in pure C
- **Web browser player**: Advanced browser-based player available at https://github.com/rewindtty/browser_player

## Interactive Mode ⚠️ Experimental

rewindtty now supports an **interactive mode** that provides a script-like experience similar to `script` and `scriptreplay` utilities. This mode allows you to record and replay terminal sessions in real-time with enhanced interactivity.

**Note**: This feature is currently experimental and may have limitations or bugs.

### Interactive vs Legacy Mode Comparison

| Feature               | Interactive Mode                            | Legacy Mode                       |
| --------------------- | ------------------------------------------- | --------------------------------- |
| **Recording Style**   | Real-time shell interaction                 | Command-by-command capture        |
| **Replay Experience** | Live terminal emulation (like scriptreplay) | Step-by-step command replay       |
| **Session Analysis**  | ❌ Not available\*                          | ✅ Full analysis with statistics  |
| **File Format**       | Enhanced JSON with timing data              | Standard JSON format              |
| **Browser Player**    | ✅ Compatible                               | ✅ Compatible                     |
| **Performance**       | Higher memory usage                         | Lightweight                       |
| **Use Case**          | Full session recording/replay               | Command analysis and optimization |

\*The analyze tool is not available in interactive mode because commands cannot be reliably stored and parsed from the raw shell interaction data.

## Building

### Prerequisites

- GCC compiler
- GNU Make
- Standard C library with GNU extensions
- Git (for cloning project and submodules)

### Cloning the repository

This project uses cJSON as a Git submodule.
Make sure to clone with submodules:

```bash
git clone --recurse-submodules https://github.com/debba/rewindtty.git
```

If you already cloned the repository without submodules, run:

```bash
git submodule update --init --recursive
```

### Compilation

```bash
make
```

This will create the executable at `build/rewindtty`.

To clean build artifacts:

```bash
make clean
```

## Usage

### Recording a Session

To start recording a terminal session:

```bash
./build/rewindtty record [--interactive] [file]
```

This will create a new session file (defaults to `data/session.json` if no file is specified) and begin capturing all terminal activity.

### Replaying a Session

To replay a previously recorded session:

```bash
./build/rewindtty replay [file]
```

This will read the session file (defaults to `data/session.json` if no file is specified) and replay it with the original timing.

### Analyzing a Session

To analyze a recorded session and get detailed statistics:

```bash
./build/rewindtty analyze [file]
```

This will generate a comprehensive analysis report including:

- Total commands executed and session duration
- Average time per command
- Most frequently used commands
- Slowest commands
- Commands that generated errors or warnings
- Helpful suggestions for optimization

### Command Line Options

```
Usage: rewindtty [record|replay|analyze] [file]

Commands:
  record [file]    Start recording a new terminal session to specified file (default: data/session.json)
  replay [file]    Replay a recorded session from specified file (default: data/session.json)
  analyze [file]   Analyze a recorded session and generate statistics report (default: data/session.json)
```

## Browser Player

For an advanced web-based player with enhanced features and a modern interface, visit: https://github.com/rewindtty/browser_player

You can try the player here: https://play.rewindtty.dev

## File Structure

```
rewindtty/
├── src/
│   ├── main.c          # Main program entry point
│   ├── recorder.c      # Session recording functionality
│   ├── recorder.h      # Recording function declarations
│   ├── replayer.c      # Session replay functionality
│   ├── replayer.h      # Replay function declarations
│   ├── analyzer.c      # Session analysis functionality
│   ├── analyzer.h      # Analysis function declarations
│   ├── utils.c         # Utility functions
│   └── utils.h         # Utility function declarations
├── data/
│   └── session.json    # Default session storage file
├── build/              # Build output directory
├── assets/
│   └── demo.gif        # Demo animation
├── libs/
│   └── cjson/          # JSON parsing library
├── LICENSE             # MIT License
├── Makefile           # Build configuration
└── README.md          # This file
```

## Session File Format

Sessions are stored in JSON format in the `data/session.json` file. The format captures timing information and terminal data to enable accurate replay.

## Signal Handling

The recorder handles interruption signals (like Ctrl+C) gracefully by:

- Closing the session file properly
- Writing the final JSON structure
- Cleaning up resources before exit

## Development

### Compiler Flags

The project uses the following GCC flags:

- `-Wall -Wextra`: Enable comprehensive warnings
- `-std=gnu99`: Use GNU C99 standard
- `-g`: Include debugging symbols

### Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

Copyright (c) 2025 Andrea Debernardi

## Technical Notes

- Uses standard POSIX system calls for terminal interaction
- Implements proper signal handling for clean shutdown
- JSON output format enables integration with other tools
- Minimal memory footprint and dependencies
