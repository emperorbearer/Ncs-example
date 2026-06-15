from fastapi import APIRouter, Depends, HTTPException
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from sqlalchemy.orm import Session
from database import get_db
from models import Teacher
from schemas import TeacherCreate, TeacherLogin, Token
from passlib.context import CryptContext
from jose import JWTError, jwt
from datetime import datetime, timedelta
import os

router = APIRouter()
security = HTTPBearer()

SECRET_KEY = os.getenv("SECRET_KEY", "dev-secret-key-change-in-production-!!!")
ALGORITHM = "HS256"
TOKEN_EXPIRE_HOURS = 24

pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")


def hash_password(password: str) -> str:
    return pwd_context.hash(password)


def verify_password(plain: str, hashed: str) -> bool:
    return pwd_context.verify(plain, hashed)


def create_token(teacher_id: int) -> str:
    expire = datetime.utcnow() + timedelta(hours=TOKEN_EXPIRE_HOURS)
    return jwt.encode({"sub": str(teacher_id), "exp": expire}, SECRET_KEY, algorithm=ALGORITHM)


def get_current_teacher(
    credentials: HTTPAuthorizationCredentials = Depends(security),
    db: Session = Depends(get_db)
) -> Teacher:
    try:
        payload = jwt.decode(credentials.credentials, SECRET_KEY, algorithms=[ALGORITHM])
        teacher_id = int(payload["sub"])
    except (JWTError, KeyError, ValueError):
        raise HTTPException(status_code=401, detail="유효하지 않은 토큰입니다")
    teacher = db.query(Teacher).filter(Teacher.id == teacher_id).first()
    if not teacher:
        raise HTTPException(status_code=401, detail="교사를 찾을 수 없습니다")
    return teacher


@router.post("/register", response_model=Token)
def register(data: TeacherCreate, db: Session = Depends(get_db)):
    if db.query(Teacher).filter(Teacher.email == data.email).first():
        raise HTTPException(status_code=400, detail="이미 등록된 이메일입니다")
    teacher = Teacher(
        name=data.name,
        email=data.email,
        hashed_password=hash_password(data.password)
    )
    db.add(teacher)
    db.commit()
    db.refresh(teacher)
    return Token(
        access_token=create_token(teacher.id),
        token_type="bearer",
        teacher_name=teacher.name
    )


@router.post("/login", response_model=Token)
def login(data: TeacherLogin, db: Session = Depends(get_db)):
    teacher = db.query(Teacher).filter(Teacher.email == data.email).first()
    if not teacher or not verify_password(data.password, teacher.hashed_password):
        raise HTTPException(status_code=401, detail="이메일 또는 비밀번호가 올바르지 않습니다")
    return Token(
        access_token=create_token(teacher.id),
        token_type="bearer",
        teacher_name=teacher.name
    )


@router.get("/me")
def get_me(teacher: Teacher = Depends(get_current_teacher)):
    return {"id": teacher.id, "name": teacher.name, "email": teacher.email}
