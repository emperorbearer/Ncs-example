from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from database import get_db
from models import Class, Student, Presentation, Teacher
from schemas import PresentationCreate, PresentationOut, PresentationWithScores
from routers.auth import get_current_teacher
from typing import List
from datetime import datetime

router = APIRouter()


@router.post("/", response_model=PresentationOut)
def start_presentation(
    data: PresentationCreate,
    teacher: Teacher = Depends(get_current_teacher),
    db: Session = Depends(get_db)
):
    student = db.query(Student).filter(Student.id == data.presenter_id).first()
    if not student:
        raise HTTPException(status_code=404, detail="학생을 찾을 수 없습니다")
    class_ = db.query(Class).filter(
        Class.id == student.class_id, Class.teacher_id == teacher.id
    ).first()
    if not class_:
        raise HTTPException(status_code=403, detail="권한이 없습니다")

    # End any active presentations in this class
    for p in db.query(Presentation).filter(
        Presentation.class_id == student.class_id, Presentation.is_active == True
    ).all():
        p.is_active = False
        p.ended_at = datetime.utcnow()

    presentation = Presentation(
        title=data.title,
        presenter_id=data.presenter_id,
        class_id=student.class_id,
        is_active=True
    )
    db.add(presentation)
    db.commit()
    db.refresh(presentation)
    return presentation


@router.put("/{presentation_id}/end", response_model=PresentationOut)
def end_presentation(
    presentation_id: int,
    teacher: Teacher = Depends(get_current_teacher),
    db: Session = Depends(get_db)
):
    presentation = db.query(Presentation).filter(Presentation.id == presentation_id).first()
    if not presentation:
        raise HTTPException(status_code=404, detail="발표를 찾을 수 없습니다")
    class_ = db.query(Class).filter(
        Class.id == presentation.class_id, Class.teacher_id == teacher.id
    ).first()
    if not class_:
        raise HTTPException(status_code=403, detail="권한이 없습니다")
    presentation.is_active = False
    presentation.ended_at = datetime.utcnow()
    db.commit()
    db.refresh(presentation)
    return presentation


@router.get("/active/{access_code}")
def get_active_by_code(access_code: str, db: Session = Depends(get_db)):
    class_ = db.query(Class).filter(Class.access_code == access_code).first()
    if not class_:
        raise HTTPException(status_code=404, detail="반을 찾을 수 없습니다")
    presentation = db.query(Presentation).filter(
        Presentation.class_id == class_.id, Presentation.is_active == True
    ).first()
    if not presentation:
        return {"active": False, "class_id": class_.id, "presentation": None}
    return {
        "active": True,
        "class_id": class_.id,
        "presentation": {
            "id": presentation.id,
            "title": presentation.title,
            "presenter_name": presentation.presenter.name,
            "presenter_id": presentation.presenter_id,
            "started_at": presentation.created_at.isoformat()
        }
    }


@router.get("/class/{class_id}/active")
def get_active_by_class(class_id: int, db: Session = Depends(get_db)):
    presentation = db.query(Presentation).filter(
        Presentation.class_id == class_id, Presentation.is_active == True
    ).first()
    if not presentation:
        return {"active": False, "presentation": None}
    return {
        "active": True,
        "presentation": {
            "id": presentation.id,
            "title": presentation.title,
            "presenter_name": presentation.presenter.name,
            "presenter_id": presentation.presenter_id,
            "started_at": presentation.created_at.isoformat()
        }
    }


@router.get("/class/{class_id}/results", response_model=List[PresentationWithScores])
def get_class_results(
    class_id: int,
    teacher: Teacher = Depends(get_current_teacher),
    db: Session = Depends(get_db)
):
    class_ = db.query(Class).filter(
        Class.id == class_id, Class.teacher_id == teacher.id
    ).first()
    if not class_:
        raise HTTPException(status_code=403, detail="권한이 없습니다")

    presentations = db.query(Presentation).filter(
        Presentation.class_id == class_id
    ).order_by(Presentation.created_at.desc()).all()

    result = []
    for p in presentations:
        scores = p.scores
        avg = round(sum(s.score for s in scores) / len(scores), 1) if scores else None
        result.append(PresentationWithScores(
            id=p.id, title=p.title, presenter_id=p.presenter_id,
            class_id=p.class_id, is_active=p.is_active,
            created_at=p.created_at, ended_at=p.ended_at,
            presenter=p.presenter, scores=scores,
            average_score=avg, score_count=len(scores)
        ))
    return result
