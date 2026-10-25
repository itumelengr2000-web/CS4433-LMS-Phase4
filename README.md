# Moderated LMS — Phase 4 Implementation

This is the Phase 4 implementation foundation for the moderated Learning Management System described in the CS4433 brief and Phase 3 design. It uses C++17, CMake, SQLite, OpenSSL PBKDF2-HMAC-SHA256, GoogleTest, Git, and cppcheck.

## Phase 2 alignment

The valid Phase 2 document has now been received. It defines FR-01–FR-14 and NFR-01–NFR-06, all prioritised as Must. The implementation must preserve late submissions, calculate `ceil((receipt time - effective deadline) / 24 hours)`, apply the 10%-of-maximum-mark deduction for days 1–3, apply zero after 72 hours, and round the final mark to two decimals. The HoD must approve every mark in the selected assessment batch before release. See `requirements-traceability.csv` for the current mapping.

The Phase 2 document repeats FR-10 twice; this project treats the repeated entry as one requirement and does not create a second implementation requirement.

## Step-by-step setup

1. **Create the project folder and enter it.**
   ```bash
   cd /home/ubuntu/lms-phase4
   ```

2. **Check the tools.**
   ```bash
   c++ --version
   cmake --version
   sqlite3 --version
   cppcheck --version
   ```

3. **Configure an out-of-source build.**
   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
   ```

4. **Compile the application and tests.**
   ```bash
   cmake --build build
   ```

5. **Run the automated tests.**
   ```bash
   ctest --test-dir build --output-on-failure
   ```

6. **Create the local database.** Run from the project root so `schema.sql` is found:
   ```bash
   ./build/lms_app lms.db
   sqlite3 lms.db '.tables'
   ```
   Never commit `lms.db`; it may contain personal or assessment data.

7. **Run static analysis.**
   ```bash
   cppcheck --enable=warning,style,performance,portability \
     --std=c++17 --inline-suppr --error-exitcode=1 \
     -I include src tests
   ```

8. **Start Git configuration management.**
   ```bash
   git init
   git add CMakeLists.txt README.md schema.sql include src tests
   git commit -m "Build Phase 4 C++ SQLite foundation"
   git branch -M main
   ```
   Use `feature/<short-name>` branches for each workflow and merge them into `main` only after tests pass.

## What is implemented in this foundation

- Layered-compatible core library structure.
- SQLite database wrapper with foreign keys enabled and exception-based failures.
- Phase 3 schema with role, course, assignment, extension, submission-version, mark, moderation, and audit tables.
- Strategy pattern for the Head of Department's three-day, 10%-per-day late penalty.
- Abstract `User` class with polymorphic Student, Lecturer, HoD, and Administrator roles.
- Factory pattern for constructing role objects.
- Salted password hashes using OpenSSL PBKDF2-HMAC-SHA256; plaintext passwords are never stored.
- GoogleTest checks for the late-penalty and password-hashing rules.
- Initial FR/NFR traceability matrix from the valid Phase 2 SRS.
- Authentication foundation: parameterised account lookup, password verification, role construction, inactive-account rejection, and session sign-out.
- Administrator account foundation: create, edit, and deactivate accounts with validation, salted password storage, and linked-record preservation.
- Administrator academic foundation: departments, courses, lecturer assignment, student enrolment, and withdrawal with parameterised SQLite operations.
- Lecturer assignment foundation: scoped assignment creation/editing, activation, deadline and mark validation, and individual extensions with original-deadline tracking.
- Student workflow foundation: enrolled active-assignment viewing, versioned file submissions, SHA-256 checksums, extension-aware deadlines, and late classification.
- Lecturer marking foundation: scoped mark entry, raw-mark validation, feedback persistence, exact late-day calculation, final-mark calculation, and mark-change audit entries.
- HoD moderation foundation: department scope checks, batch decisions, complete-batch release blocking, post-release student visibility, and moderation/release audit entries.
- Console UI foundation: login shell, role-specific menu routing, and ANSI colour palette with gray Administrator/title styling.
- Lecturer UI slice: assignment creation/editing, activation, extensions, submission listing, and mark-entry menu actions.
- Student UI slice: active-assignment viewing, file submission/resubmission, submission status, and released-mark viewing.
- HoD UI slice: batch opening/listing, mark review, approval/rejection, and complete-batch release.

## Security and design justification

Passwords are stored using OpenSSL PBKDF2-HMAC-SHA256 with a cryptographically random 16-byte salt and 210,000 derivation iterations. PBKDF2 is a recognised password-based key-derivation algorithm, the per-password salt prevents identical passwords producing identical stored values, and OpenSSL provides a maintained cryptographic implementation. Password plaintext is never written to SQLite. Verification uses a constant-time comparison.

The Factory pattern constructs the polymorphic `User` subclasses required by the Phase 3 class design. The Strategy pattern encapsulates the late-penalty calculation so the policy can be changed without changing marking workflow code. SQLite access is kept in the database/repository/service layer; the console UI calls services and does not execute SQL.

The audit log records the actor, current UTC timestamp, entity and action, old value, new value, reason, and transaction result for mark changes and moderation decisions. Marking and moderation writes are atomic: an exception rolls the transaction back.

## Coding standard and quality checks

The project uses C++17, four-space indentation, lower-case `snake_case` file names, PascalCase class names, descriptive typed interfaces, RAII/exception-based error handling, prepared SQLite statements, and no plaintext secrets. The build enables `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`. Run cppcheck before committing:

```bash
cppcheck --enable=warning,style,performance,portability \
  --std=c++17 --inline-suppr --error-exitcode=1 \
  -I include src tests
```

The schema includes indexes for the principal course, lecturer, enrolment, submission, marking, moderation, and audit lookup paths. An assessor can inspect the index use with:

```bash
sqlite3 final-demo.db "EXPLAIN QUERY PLAN SELECT * FROM submission WHERE student_id = 3;"
```

## Evidence to save for the report

Keep terminal output or screenshots for: successful CMake build, passing CTest run, database tables, static-analysis result, Git history, each role dashboard, assignment creation, file submission, marking, moderation rejection/approval, release, and post-release student viewing. These become Phase 4, Phase 5, and Phase 6 evidence. The `END_TO_END_DEMO.md` file contains the exact demonstration sequence.
