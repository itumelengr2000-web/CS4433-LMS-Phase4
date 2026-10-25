# LMS Phase 4 End-to-End Demonstration

This guide demonstrates the complete workflow across all four roles:

1. Administrator creates the academic structure and user accounts.
2. Lecturer creates an assignment.
3. Student submits a file.
4. Lecturer views and marks the submission.
5. HoD opens the moderation batch, approves the mark, and releases it.
6. Student views the released mark and feedback.

The application is a C++17 console application running in Ubuntu through WSL2.

## 1. Build and test

From the project directory:

```bash
cd ~/projects/lms-phase4
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected result:

```text
100% tests passed, 0 tests failed out of 33
```

## 2. Create a demonstration database

Use a fresh database for the final demonstration:

```bash
./build/lms_app final-demo.db --create-admin
```

Enter:

```text
Username: admin
Password: AdminPass123
```

The password must contain at least eight characters. Do not commit the database file to Git.

Start the application:

```bash
./build/lms_app final-demo.db
```

Log in as `admin`.

## 3. Administrator setup

Use the gray Administrator dashboard.

### Create a Lecturer

Select option `1`:

```text
Username: lecturer1
Password: LecturerPass123
Role: 2
```

Record the displayed Lecturer user ID.

### Create a Student

Select option `1` again:

```text
Username: student1
Password: StudentPass123
Role: 1
```

Record the displayed Student user ID.

### Create a HoD

Select option `1` again:

```text
Username: hod1
Password: HodPass123
Role: 3
```

Record the displayed HoD user ID.

### Create a department

Select option `4`:

```text
Department name: Computing
```

Record the Department ID.

### Assign the HoD to the department

Select option `9`:

```text
Department ID: <department ID>
HoD user ID: <HoD user ID>
```

### Create a course

Select option `5`:

```text
Course code: CS4433
Course name: Software Engineering
Department ID: <department ID>
```

Record the Course ID.

### Assign the Lecturer

Select option `6`:

```text
Course ID: <course ID>
Lecturer user ID: <Lecturer user ID>
```

### Enrol the Student

Select option `7`:

```text
Course ID: <course ID>
Student user ID: <Student user ID>
```

Select option `0` to sign out.

## 4. Lecturer creates an assignment

Log in as `lecturer1`.

The cyan Lecturer dashboard appears. Select option `1`:

```text
Course ID: <course ID>
Title: Final Project
Instructions: Submit the source code and report.
Effective due timestamp: 2026-11-07T23:59:59Z
Maximum mark: 100
```

Record the Assignment ID.

Sign out with option `0`.

## 5. Student submits a file

Before logging in as the Student, create a sample file from Ubuntu:

```bash
printf 'Sample CS4433 final project submission\n' > ~/projects/sample-submission.txt
```

Log in as `student1` and select option `1` to confirm that the assignment is visible.

Select option `2`:

```text
Assignment ID: <assignment ID>
Path to file: /home/ubuntu/projects/sample-submission.txt
```

The system records the file as a versioned submission and calculates its SHA-256 checksum.

Expected result:

```text
Submission version 1 saved as ON TIME.
```

Select option `3` to view the submission status, then sign out.

## 6. Lecturer views and marks the submission

Log in as `lecturer1`.

Select option `5` to view submissions. Record the Submission ID and Version ID.

Select option `6`:

```text
Submission ID: <submission ID>
Version ID: <version ID>
Raw mark: 82
Written feedback: Good work. Improve the testing explanation.
```

The system calculates and stores the final mark, applies the late penalty when necessary, updates the submission status, and writes an audit record.

Sign out.

## 7. HoD moderates and releases the mark

Log in as `hod1`.

Select option `1`. The system shows the department batches. Enter the Assignment ID to open the assessment batch.

Record the Batch ID.

Select option `2` and enter the Batch ID. Record the Mark ID.

Select option `3`:

```text
Batch ID: <batch ID>
Mark ID: <mark ID>
Approval reason: Mark checked and approved.
```

Select option `5`:

```text
Batch ID: <batch ID>
```

The batch is released only when every mark in the batch has been approved. If any mark is pending or rejected, release is refused.

Sign out.

## 8. Student views the released result

Log in as `student1`.

Select option `4`:

```text
Assignment ID: <assignment ID>
```

The Student can now see:

- Raw mark
- Final mark after any late penalty
- Written feedback

Before release, the same action displays:

```text
This mark has not been released yet.
```

## 9. Useful checks

Run the full automated tests again:

```bash
ctest --test-dir build --output-on-failure
```

Check the database tables:

```bash
sqlite3 final-demo.db ".tables"
```

Check the recorded users:

```bash
sqlite3 final-demo.db "SELECT user_id, username, role, active FROM user_account;"
```

Check assignments:

```bash
sqlite3 final-demo.db "SELECT assignment_id, course_id, lecturer_id, title, active FROM assignment;"
```

Check submissions:

```bash
sqlite3 final-demo.db "SELECT submission_id, assignment_id, student_id, status FROM submission;"
```

Do not commit `final-demo.db`; it is ignored by `.gitignore`.

## 10. Evidence to capture for the report or viva

Capture screenshots of:

1. The gray Administrator dashboard.
2. The cyan Lecturer assignment menu.
3. The green Student assignment and submission menu.
4. The magenta HoD moderation menu.
5. A successful assignment creation.
6. A successful Student submission showing the version and late status.
7. A Lecturer mark and feedback entry.
8. HoD approval and release.
9. Student viewing the released final mark and feedback.
10. The terminal showing `100% tests passed, 0 tests failed out of 33`.
