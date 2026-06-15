from sqlalchemy import Column, Integer, String, Float, ForeignKey, DateTime, Boolean, Text
from sqlalchemy.orm import relationship
from database import Base
from datetime import datetime
import uuid


class Teacher(Base):
    __tablename__ = "teachers"
    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(100), nullable=False)
    email = Column(String(200), unique=True, index=True, nullable=False)
    hashed_password = Column(String(200), nullable=False)
    created_at = Column(DateTime, default=datetime.utcnow)
    classes = relationship("Class", back_populates="teacher", cascade="all, delete-orphan")


class Class(Base):
    __tablename__ = "classes"
    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(200), nullable=False)
    description = Column(Text, nullable=True)
    teacher_id = Column(Integer, ForeignKey("teachers.id"), nullable=False)
    access_code = Column(String(50), unique=True, index=True)
    created_at = Column(DateTime, default=datetime.utcnow)
    teacher = relationship("Teacher", back_populates="classes")
    students = relationship("Student", back_populates="class_", cascade="all, delete-orphan")
    presentations = relationship("Presentation", back_populates="class_", cascade="all, delete-orphan")

    def __init__(self, **kwargs):
        if not kwargs.get("access_code"):
            kwargs["access_code"] = str(uuid.uuid4())[:8].upper()
        super().__init__(**kwargs)


class Student(Base):
    __tablename__ = "students"
    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(100), nullable=False)
    student_number = Column(String(50), nullable=True)
    class_id = Column(Integer, ForeignKey("classes.id"), nullable=False)
    class_ = relationship("Class", back_populates="students")
    scores_given = relationship("Score", foreign_keys="Score.evaluator_id", back_populates="evaluator")
    presentations = relationship("Presentation", back_populates="presenter")


class Presentation(Base):
    __tablename__ = "presentations"
    id = Column(Integer, primary_key=True, index=True)
    title = Column(String(300), nullable=True)
    presenter_id = Column(Integer, ForeignKey("students.id"), nullable=False)
    class_id = Column(Integer, ForeignKey("classes.id"), nullable=False)
    is_active = Column(Boolean, default=True)
    created_at = Column(DateTime, default=datetime.utcnow)
    ended_at = Column(DateTime, nullable=True)
    presenter = relationship("Student", back_populates="presentations")
    class_ = relationship("Class", back_populates="presentations")
    scores = relationship("Score", back_populates="presentation", cascade="all, delete-orphan")


class Score(Base):
    __tablename__ = "scores"
    id = Column(Integer, primary_key=True, index=True)
    score = Column(Float, nullable=False)
    comment = Column(Text, nullable=True)
    presentation_id = Column(Integer, ForeignKey("presentations.id"), nullable=False)
    evaluator_id = Column(Integer, ForeignKey("students.id"), nullable=False)
    created_at = Column(DateTime, default=datetime.utcnow)
    presentation = relationship("Presentation", back_populates="scores")
    evaluator = relationship("Student", foreign_keys=[evaluator_id], back_populates="scores_given")
