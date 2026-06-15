from pydantic import BaseModel
from typing import Optional, List
from datetime import datetime


class TeacherCreate(BaseModel):
    name: str
    email: str
    password: str


class TeacherLogin(BaseModel):
    email: str
    password: str


class Token(BaseModel):
    access_token: str
    token_type: str
    teacher_name: str


class StudentOut(BaseModel):
    id: int
    name: str
    student_number: Optional[str] = None
    class_id: int
    model_config = {"from_attributes": True}


class ClassCreate(BaseModel):
    name: str
    description: Optional[str] = None


class ClassOut(BaseModel):
    id: int
    name: str
    description: Optional[str] = None
    access_code: str
    teacher_id: int
    created_at: datetime
    students: List[StudentOut] = []
    model_config = {"from_attributes": True}


class ClassSummary(BaseModel):
    id: int
    name: str
    description: Optional[str] = None
    access_code: str
    student_count: int
    created_at: datetime


class PresentationCreate(BaseModel):
    presenter_id: int
    title: Optional[str] = None


class PresentationOut(BaseModel):
    id: int
    title: Optional[str] = None
    presenter_id: int
    class_id: int
    is_active: bool
    created_at: datetime
    ended_at: Optional[datetime] = None
    presenter: Optional[StudentOut] = None
    model_config = {"from_attributes": True}


class ScoreCreate(BaseModel):
    score: float
    comment: Optional[str] = None
    presentation_id: int
    evaluator_id: int


class ScoreOut(BaseModel):
    id: int
    score: float
    comment: Optional[str] = None
    presentation_id: int
    evaluator_id: int
    created_at: datetime
    evaluator: Optional[StudentOut] = None
    model_config = {"from_attributes": True}


class PresentationWithScores(PresentationOut):
    scores: List[ScoreOut] = []
    average_score: Optional[float] = None
    score_count: int = 0
