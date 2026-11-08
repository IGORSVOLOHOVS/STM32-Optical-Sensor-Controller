# Install

## On the board

The firmware runs on a NUCLEO-C092RC (STM32C092RC).

1. Install STM32CubeIDE 1.19 or newer; it brings the arm toolchain.
2. Clone this repository.
3. In STM32CubeIDE: `File > Import > General > Existing Projects into Workspace`, select the repository root. To regenerate the CubeMX code, open `OpticManager.ioc`.
4. Choose the transport: `Core/Src/Impl/main.cpp` defines `USE_UART_TRANSPORT`; remove that line for CAN.
5. `Project > Build All`, then flash over the board's ST-LINK with `Run > Debug`.

The debug log arrives on the ST-LINK virtual COM port at 115200 8N1.

## Here, for the tests

1. Install mingw g++ 13 or newer, CMake 3.20 or newer, Ninja, LLVM for `clang-tidy` and `clang-format`, and Python with pytest.
2. Clone this repository.
3. Run `.\recipe.ps1 test`. Nothing is downloaded: the tests use no framework.

`recipe.ps1` looks for g++ in `C:\ProgramData\mingw64\mingw64\bin` and for LLVM in `C:\Program Files\LLVM\bin`, and adds those folders to its own PATH.

`.\recipe.ps1 build` compiles the tests into `build\`, and nothing else: the rest of `Core/` needs the STM32 HAL and the arm toolchain. `.\recipe.ps1 install` and `.\recipe.ps1 run` do nothing here: there is no installer, and the firmware runs on the board.
