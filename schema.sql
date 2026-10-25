PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS department (
  department_id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE
);
CREATE TABLE IF NOT EXISTS user_account (
  user_id INTEGER PRIMARY KEY,
  username TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  role TEXT NOT NULL CHECK (role IN ('STUDENT','LECTURER','HOD','ADMIN')),
  active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
  created_at TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS student_profile (
  user_id INTEGER PRIMARY KEY REFERENCES user_account(user_id),
  student_number TEXT NOT NULL UNIQUE
);
CREATE TABLE IF NOT EXISTS lecturer_profile (
  user_id INTEGER PRIMARY KEY REFERENCES user_account(user_id),
  staff_number TEXT NOT NULL UNIQUE
);
CREATE TABLE IF NOT EXISTS hod_department (
  hod_user_id INTEGER NOT NULL REFERENCES user_account(user_id),
  department_id INTEGER NOT NULL REFERENCES department(department_id),
  PRIMARY KEY (hod_user_id, department_id)
);
CREATE TABLE IF NOT EXISTS course (
  course_id INTEGER PRIMARY KEY,
  course_code TEXT NOT NULL UNIQUE,
  name TEXT NOT NULL,
  department_id INTEGER NOT NULL REFERENCES department(department_id)
);
CREATE TABLE IF NOT EXISTS course_lecturer (
  course_id INTEGER NOT NULL REFERENCES course(course_id),
  lecturer_id INTEGER NOT NULL REFERENCES lecturer_profile(user_id),
  assigned_at TEXT NOT NULL,
  PRIMARY KEY (course_id, lecturer_id)
);
CREATE TABLE IF NOT EXISTS enrolment (
  course_id INTEGER NOT NULL REFERENCES course(course_id),
  student_id INTEGER NOT NULL REFERENCES student_profile(user_id),
  enrolled_at TEXT NOT NULL,
  withdrawn_at TEXT,
  PRIMARY KEY (course_id, student_id)
);
CREATE TABLE IF NOT EXISTS assignment (
  assignment_id INTEGER PRIMARY KEY,
  course_id INTEGER NOT NULL REFERENCES course(course_id),
  lecturer_id INTEGER NOT NULL REFERENCES lecturer_profile(user_id),
  title TEXT NOT NULL,
  instructions TEXT NOT NULL,
  effective_due_at TEXT NOT NULL,
  maximum_mark REAL NOT NULL CHECK (maximum_mark > 0),
  active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1))
);
CREATE TABLE IF NOT EXISTS extension (
  extension_id INTEGER PRIMARY KEY,
  assignment_id INTEGER NOT NULL REFERENCES assignment(assignment_id),
  student_id INTEGER NOT NULL REFERENCES student_profile(user_id),
  original_due_at TEXT NOT NULL,
  new_due_at TEXT NOT NULL,
  reason TEXT NOT NULL,
  granted_by INTEGER NOT NULL REFERENCES lecturer_profile(user_id),
  granted_at TEXT NOT NULL,
  UNIQUE (assignment_id, student_id)
);
CREATE TABLE IF NOT EXISTS submission (
  submission_id INTEGER PRIMARY KEY,
  assignment_id INTEGER NOT NULL REFERENCES assignment(assignment_id),
  student_id INTEGER NOT NULL REFERENCES student_profile(user_id),
  status TEXT NOT NULL CHECK (status IN ('SUBMITTED','LATE','MARKED','HELD','RELEASED')),
  UNIQUE (assignment_id, student_id)
);
CREATE TABLE IF NOT EXISTS submission_version (
  version_id INTEGER PRIMARY KEY,
  submission_id INTEGER NOT NULL REFERENCES submission(submission_id),
  version_no INTEGER NOT NULL CHECK (version_no > 0),
  received_at TEXT NOT NULL,
  storage_path TEXT NOT NULL,
  checksum TEXT NOT NULL,
  is_late INTEGER NOT NULL CHECK (is_late IN (0,1)),
  UNIQUE (submission_id, version_no)
);
CREATE TABLE IF NOT EXISTS mark (
  mark_id INTEGER PRIMARY KEY,
  submission_id INTEGER NOT NULL UNIQUE REFERENCES submission(submission_id),
  version_id INTEGER NOT NULL REFERENCES submission_version(version_id),
  raw_mark REAL NOT NULL CHECK (raw_mark >= 0),
  final_mark REAL NOT NULL CHECK (final_mark >= 0),
  feedback TEXT NOT NULL,
  marked_by INTEGER NOT NULL REFERENCES lecturer_profile(user_id),
  marked_at TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS assessment_batch (
  batch_id INTEGER PRIMARY KEY,
  assignment_id INTEGER NOT NULL UNIQUE REFERENCES assignment(assignment_id),
  status TEXT NOT NULL CHECK (status IN ('OPEN','APPROVED','RELEASED')),
  released_at TEXT
);
CREATE TABLE IF NOT EXISTS moderation_decision (
  decision_id INTEGER PRIMARY KEY,
  batch_id INTEGER NOT NULL REFERENCES assessment_batch(batch_id),
  mark_id INTEGER NOT NULL REFERENCES mark(mark_id),
  hod_user_id INTEGER NOT NULL REFERENCES user_account(user_id),
  decision TEXT NOT NULL CHECK (decision IN ('APPROVED','REJECTED')),
  reason TEXT NOT NULL,
  decided_at TEXT NOT NULL,
  UNIQUE (batch_id, mark_id)
);
CREATE TABLE IF NOT EXISTS audit_log (
  audit_id INTEGER PRIMARY KEY,
  changed_by INTEGER NOT NULL REFERENCES user_account(user_id),
  entity_type TEXT NOT NULL,
  entity_id INTEGER NOT NULL,
  action TEXT NOT NULL,
  old_value TEXT,
  new_value TEXT,
  reason TEXT NOT NULL,
  changed_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_course_department ON course(department_id);
CREATE INDEX IF NOT EXISTS idx_course_lecturer_lecturer ON course_lecturer(lecturer_id);
CREATE INDEX IF NOT EXISTS idx_enrolment_student ON enrolment(student_id);
CREATE INDEX IF NOT EXISTS idx_assignment_lecturer ON assignment(lecturer_id);
CREATE INDEX IF NOT EXISTS idx_assignment_course ON assignment(course_id);
CREATE INDEX IF NOT EXISTS idx_submission_student ON submission(student_id);
CREATE INDEX IF NOT EXISTS idx_submission_assignment ON submission(assignment_id);
CREATE INDEX IF NOT EXISTS idx_submission_version_submission ON submission_version(submission_id);
CREATE INDEX IF NOT EXISTS idx_mark_submission ON mark(submission_id);
CREATE INDEX IF NOT EXISTS idx_moderation_batch ON moderation_decision(batch_id);
CREATE INDEX IF NOT EXISTS idx_audit_entity ON audit_log(entity_type, entity_id);
