from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from database import get_db
from models import Score, Presentation, Student
from schemas import ScoreCreate, ScoreOut

router = APIRouter()


@router.post("/", response_model=ScoreOut)
def submit_score(data: ScoreCreate, db: Session = Depends(get_db)):
    if not (1 <= data.score <= 10):
        raise HTTPException(status_code=400, detail="점수는 1~10 사이여야 합니다")

    presentation = db.query(Presentation).filter(
        Presentation.id == data.presentation_id,
        Presentation.is_active == True
    ).first()
    if not presentation:
        raise HTTPException(status_code=404, detail="진행 중인 발표를 찾을 수 없습니다")

    evaluator = db.query(Student).filter(Student.id == data.evaluator_id).first()
    if not evaluator:
        raise HTTPException(status_code=404, detail="학생을 찾을 수 없습니다")

    if data.evaluator_id == presentation.presenter_id:
        raise HTTPException(status_code=400, detail="자신의 발표는 평가할 수 없습니다")

    existing = db.query(Score).filter(
        Score.presentation_id == data.presentation_id,
        Score.evaluator_id == data.evaluator_id
    ).first()
    if existing:
        raise HTTPException(status_code=400, detail="이미 이 발표를 평가하셨습니다")

    score = Score(
        score=data.score,
        comment=data.comment,
        presentation_id=data.presentation_id,
        evaluator_id=data.evaluator_id
    )
    db.add(score)
    db.commit()
    db.refresh(score)
    return score
