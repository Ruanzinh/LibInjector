# Lib Injector by Ruanzinh

A lightweight and robust shared library (`.so`) injector for Android, based on `ptrace`. Designed to work in Root environments (Termux, MT Manager, KernelSU), bypassing common restrictions such as `noexec` and SELinux.

This project includes a **Builder** that generates a single standalone `.sh` script containing the injector binary embedded via Base64, ensuring flawless execution without corruption errors or external dependencies.

## Project Structure

```text
.
├── libinjector.sh         # Final generated file, ready to use
├── libinjector-DIRECT-INJECT.sh  # Same thing, but you don't need to set the package every time you start the script.
└── src/
    ├── injector.cpp   # C++ source code (Injection engine)
    ├── injector       # Compiled binary (AArch64)
    └── builder.sh     # Script to generate the standalone installer
```

## Features

* Inject a shared library into a specific app

## How to Use

1. You must have **ROOT** access.
2. Run the `libinjector.sh` script (or the direct inject if u don't want to write the package name every time u start the script, just edit the script and set in line 51 ur package name.) using **MT Manager** (run as Root) or via terminal:

```bash
su -c sh libinjector.sh
```
or

```bash
su -c sh libinjector-DIRECT-INJECT.sh```

3. Enter the game package name when prompted (e.g. `com.shootergamesonline.blockstrike`), If you used the direct inject version, edit the file and modify line 51 to set your package name.

### WARNING

The default library directory is:

```bash
/sdcard/DCIM/
```

And the default library name is:

```bash
libTool.sh
```

If you want to change it:

* Modify the library name on **line 5**
* Modify the directory on **line 8**

## How to Compile and Generate (for devs)

If you modified the C++ code (`injector.cpp`) and want to generate a new script (This will only generate the normal version of libinjector, If you want to generate the Direct Inject version, just modify the builder.sh file, you're not stupid, It's an extremely simple modification.):

### 1. Requirements

* Termux (or a Linux environment with Clang)
* Packages: `clang`, `make` (optional)

### 2. Binary Compilation

Navigate to the `src` folder and compile the C++ code:

```bash
cd src
clang++ injector.cpp -o injector
```

### 3. Script Generation

Still inside the `src` folder, run `builder.sh`. It will take the binary you just compiled and create the final script:

```bash
sh builder.sh
```

The final file will be generated and can be moved to the project root.

## How It Works

1. **Shell Loader**
   The bash script decodes the binary payload (Base64) to `/data/local/tmp/`.

2. **Execution Fix**
   Sets `setenforce 0` and copies the library from `/sdcard` to `/data/local/tmp` to guarantee execution permissions. (At the end of the execution, SELinux returns to enforcing mode.)

3. **Injector (C++)**

   * Uses `ptrace(PTRACE_ATTACH)` to pause the target process
   * Locates the `dlopen` function address in the target process memory
   * Injects the library path into the stack and manipulates registers (`PC`, `X0`, `X1`, `LR`) to force the `dlopen` call
   * Restores the original process state and detaches
