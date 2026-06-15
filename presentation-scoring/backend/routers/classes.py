from fastapi import APIRouter, Depends, HTTPException, UploadFile, File
from sqlalchemy.orm import Session
from database import get_db
from models import Class, Student, Teacher
from schemas import ClassCreate, ClassOut, ClassSummary, StudentOut
from routers.auth import get_current_teacher
from typing import List
import io
import qrcode
import base64
import csv

router = APIRouter()


@router.get("/", response_model=List[ClassSummary])
def list_classes(
    teacher: Teacher = Depends(get_current_teacher),
    db: Session = Depends(get_db)
):
    classes = db.query(Class).filter(Class.teacher_id == teacher.id).all()
    return [
        ClassSummary(
            id=c.id, name=c.name, description=c.description,
            access_code=c.access_code, student_count=len(c.students),
            created_at=c.created_at
        ) for c in classes
    ]


@router.post("/", response_model=ClassOut)
def create_class(
    data: ClassCreate,
    teacher: Teacher = Depends(get_current_teacher),
    db: Session = Depends(get_db)
):
    class_ = Class(name=data.name, description=data.description, teacher_id=teacher.id)
    db.add(class_)
    db.commit()
    db.refresh(class_)
    return class_


@router.get("/public/{access_code}")
def get_class_public(access_code: str, db: Session = Depends(get_db)):
    class_ = db.query(Class).filter(Class.access_code == access_code).first()
    if not class_:
        raise HTTPException(status_code=404, detail="반을 찾을 수 없습니다")
    students = sorted(
        [
            StudentOut(id=s.id, name=s.name, student_number=s.student_number, class_id=s.class_id)
            for s in class_.students
        ],
        key=lambda s: (s.student_number or "ZZZ", s.name)
    )
    return {"class_id": class_.id, "class_name": class_.name, "students": students}


@router.get("/{class_id}", response_model=ClassOut)
def get_class(
    class_id: int,
    teacher: Teacher = Depends(get_current_teacher),
    db: Session = Depends(get_db)
):
    class_ = db.query(Class).filter(Class.id == class_id, Class.teacher_id == teacher.id).first()
    if not class_:
        raise HTTPException(status_code=404, detail="반을 찾을 수 없습니다")
    return class_


@router.delete("/{class_id}")
def delete_class(
    class_id: int,
    teacher: Teacher = Depends(get_current_teacher),
    db: Session = Depends(get_db)
):
    class_ = db.query(Class).filter(Class.id == class_id, Class.teacher_id == teacher.id).first()
    if not class_:
        raise HTTPException(status_code=404, detail="반을 찾을 수 없습니다")
    db.delete(class_)
    db.commit()
    return {"message": "삭제되었습니다"}


@router.post("/{class_id}/students/upload")
async def upload_students(
    class_id: int,
    file: UploadFile = File(...),
    teacher: Teacher = Depends(get_current_teacher),
    db: Session = Depends(get_db)
):
    class_ = db.query(Class).filter(Class.id == class_id, Class.teacher_id == teacher.id).first()
    if not class_:
        raise HTTPException(status_code=404, detail="반을 찾을 수 없습니다")
    if not file.filename.endswith(".csv"):
        raise HTTPException(status_code=400, detail="CSV 파일만 지원합니다")

    content = await file.read()
    try:
        decoded = content.decode("utf-8-sig")
    except UnicodeDecodeError:
        decoded = content.decode("cp949")

    reader = csv.DictReader(io.StringIO(decoded))
    rows = list(reader)

    db.query(Student).filter(Student.class_id == class_id).delete()

    added = 0
    for row in rows:
        name = (row.get("이름") or row.get("name") or row.get("Name") or "").strip()
        number = (
            row.get("번호") or row.get("학번") or row.get("number") or row.get("student_number") or ""
        ).strip()
        if name:
            db.add(Student(name=name, student_number=number or None, class_id=class_id))
            added += 1

    db.commit()
    return {"message": f"{added}명의 학생이 등록되었습니다"}


@router.get("/{class_id}/qr")
def get_qr_code(
    class_id: int,
    base_url: str,
    teacher: Teacher = Depends(get_current_teacher),
    db: Session = Depends(get_db)
):
    class_ = db.query(Class).filter(Class.id == class_id, Class.teacher_id == teacher.id).first()
    if not class_:
        raise HTTPException(status_code=404, detail="반을 찾을 수 없습니다")

    url = f"{base_url.rstrip('/')}/score/{class_.access_code}"
    img = qrcode.make(url)
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    img_b64 = base64.b64encode(buf.getvalue()).decode()

    return {
        "qr_data_url": f"data:image/png;base64,{img_b64}",
        "access_url": url,
        "access_code": class_.access_code
    }
