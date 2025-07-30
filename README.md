<p align="center">
  <picture>
    <img style="max-width:400px;height:auto"  src="assets/logo.png" alt="rewindtty logo">
  </picture>
</p>

A terminal session recorder and replayer written in C that allows you to capture and replay terminal sessions with precise timing.

<p align="center">
  <picture>
    <img src="assets/demo.gif" alt="rewindtty demo">
  </picture>
</p>

## Features

- **Record terminal sessions**: Capture all terminal input/output with accurate timing information
- **Replay sessions**: Play back recorded sessions with original timing
- **JSON format**: Sessions are stored in a structured JSON format for easy parsing
- **Signal handling**: Graceful shutdown and file closure on interruption
- **Lightweight**: Minimal dependencies, written in pure C

## Building

### Prerequisites

- GCC compiler
- GNU Make
- Standard C library with GNU extensions

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
./build/rewindtty record [file]
```

This will create a new session file (defaults to `data/session.json` if no file is specified) and begin capturing all terminal activity.

### Replaying a Session

To replay a previously recorded session:

```bash
./build/rewindtty replay [file]
```

This will read the session file (defaults to `data/session.json` if no file is specified) and replay it with the original timing.

### Command Line Options

```
Usage: rewindtty [record|replay] [file]

Commands:
  record [file]    Start recording a new terminal session to specified file (default: data/session.json)
  replay [file]    Replay a recorded session from specified file (default: data/session.json)
```

## File Structure

```
rewindtty/
├── src/
│   ├── main.c          # Main program entry point
│   ├── recorder.c      # Session recording functionality
│   ├── recorder.h      # Recording function declarations
│   ├── replayer.c      # Session replay functionality
│   ├── replayer.h      # Replay function declarations
│   ├── utils.c         # Utility functions
│   └── utils.h         # Utility function declarations
├── data/
│   └── session.json    # Default session storage file
├── build/              # Build output directory
├── assets/
│   └── demo.gif        # Demo animation
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
