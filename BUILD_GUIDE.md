# CS4433 Phase 4 Build and Submission Guide

## 1. Required tools

This project is a C++17 console application. On Windows, use Ubuntu through WSL2.

Install or confirm:

- Windows 10/11
- WSL2
- Ubuntu in WSL2
- Visual Studio Code with the WSL extension
- Git
- C++ compiler with C++17 support
- CMake
- Ninja
- SQLite3 development library and command-line tool
- OpenSSL development library
- GoogleTest
- cppcheck

The project was developed and tested with CMake, Ninja, SQLite, OpenSSL, and GoogleTest in Ubuntu/WSL2.

## 2. Open the project in Ubuntu

Open Ubuntu and enter the project:

```bash
cd ~/projects/lms-phase4
```

Confirm the tools:

```bash
c++ --version
cmake --version
ninja --version
sqlite3 --version
cppcheck --version
```

If dependencies are missing, install them with:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build sqlite3 libsqlite3-dev libssl-dev libgtest-dev cppcheck
```

## 3. Configure and build

Configure an out-of-source Debug build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

Compile the application and tests:

```bash
cmake --build build
```

The application executable is:

```text
build/lms_app
```

The test executable is:

```text
build/lms_tests
```

## 4. Run automated tests

```bash
ctest --test-dir build --output-on-failure
```

Expected result:

```text
100% tests passed, 0 tests failed out of 33
```

The tests cover authentication, account management, academic administration, assignment management, extensions, late penalties, submissions, marking, audit logging, moderation, release, password hashing, role scope, and transaction rollback.

## 5. Run static analysis

```bash
cppcheck --enable=warning,style,performance,portability \
  --std=c++17 --inline-suppr --error-exitcode=1 \
  -I include src tests
```

The CMake target also enables:

```text
-Wall -Wextra -Wpedantic -Wconversion -Wshadow
```

## 6. Create a demonstration database

Use a fresh database for a demonstration:

```bash
./build/lms_app final-demo.db --create-admin
```

Enter an Administrator username and password when prompted. Example:

```text
Username: admin
Password: AdminPass123
```

Start the application:

```bash
./build/lms_app final-demo.db
```

The database is local and is ignored by Git. Do not commit database files or real passwords.

## 7. Demonstrate the system

Follow [`END_TO_END_DEMO.md`](END_TO_END_DEMO.md) to demonstrate:

1. Administrator account and academic setup.
2. Lecturer assignment creation.
3. Student file submission and resubmission.
4. Lecturer marking and feedback.
5. HoD moderation and rejection/reapproval.
6. Complete-batch release.
7. Student viewing the released mark.

The project uses these console colours:

- Gray: Administrator and application title
- Cyan: Lecturer
- Green: Student
- Magenta: HoD
- Yellow: warnings and sign-out
- Red: errors
- White: normal text

## 8. Database and security checks

View the created tables:

```bash
sqlite3 final-demo.db ".tables"
```

View users without displaying passwords:

```bash
sqlite3 final-demo.db "SELECT user_id, username, role, active FROM user_account;"
```

Inspect indexes:

```bash
sqlite3 final-demo.db "SELECT name FROM sqlite_master WHERE type = 'index' ORDER BY name;"
```

Inspect an example query plan:

```bash
sqlite3 final-demo.db "EXPLAIN QUERY PLAN SELECT * FROM submission WHERE student_id = 3;"
```

Password hashes are stored using salted OpenSSL PBKDF2-HMAC-SHA256. Plain-text passwords are never stored. The database should not be submitted with the source code.

## 9. Git repository preparation

From the project root:

```bash
cd ~/projects/lms-phase4
```

Check ignored files:

```bash
git status --short --ignored
```

The following should be ignored:

- `build/`
- `*.db`
- SQLite journal files
- `.vscode/`
- `compile_commands.json`
- `.env`

Initialize Git if necessary:

```bash
git init
```

Set your identity if it has not already been configured:

```bash
git config user.name "Itumeleng"
git config user.email "itumelengr2000@gmail.com"
```

Add only source, tests, configuration, schema, and documentation:

```bash
git add .gitignore CMakeLists.txt README.md BUILD_GUIDE.md END_TO_END_DEMO.md schema.sql requirements-traceability.csv include src tests
```

Review the staged files:

```bash
git status
```

Create the submission commit:

```bash
git commit -m "Complete CS4433 Phase 4 LMS implementation"
```

Confirm the commit:

```bash
git log --oneline --stat -1
```

## 10. Optional GitHub upload

Create an empty repository on GitHub. Do not add another README or `.gitignore` if those files already exist locally.

Then, in Ubuntu, replace the placeholder URL with your repository URL:

```bash
git branch -M main
git remote add origin https://github.com/itumelengr2000-web/REPOSITORY-NAME.git
git push -u origin main
```

If GitHub asks for authentication, use the GitHub sign-in flow or a GitHub Personal Access Token instead of your account password.

## 11. Submission checklist

Before submitting, confirm that the repository contains:

- `CMakeLists.txt`
- `README.md`
- `BUILD_GUIDE.md`
- `END_TO_END_DEMO.md`
- `requirements-traceability.csv`
- `schema.sql`
- `include/`
- `src/`
- `tests/`
- A clean Git commit

Confirm that it does not contain:

- `build/`
- `.db` files
- Passwords
- Personal secrets
- `.env` files
- Temporary archives

Keep screenshots or terminal evidence showing the successful build, the 33 passing tests, cppcheck, the four dashboards, and the complete end-to-end workflow for the written Phase 4 submission.
