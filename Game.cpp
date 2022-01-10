#include <QApplication>
#include <QPainter>
#include <QGraphicsPixmapItem>
#include <QGraphicsProxyWidget>
#include <QPushButton>
#include <QKeyEvent>
#include <QIcon>
#include <cmath>
#include <qDebug>
#include <set>


#include "Game.h"
#include "Peg.h"
#include "Bucket.h"
#include "MasterPeg.h"
#include "QJsonObject"
#include "QJsonDocument"
#include "Button.h"



#include "Sounds.h"
#include "Sprites.h"

#include "box2d/include/box2d/b2_settings.h"
#include "box2d/include/box2d/box2d.h"

using namespace PGG;

// singleton
Game* Game::_uniqueInstance = 0;
Game* Game::instance()
{
    if (_uniqueInstance == 0)
        _uniqueInstance = new Game();
    return _uniqueInstance;
}

Game::Game() : QGraphicsView()
{
    // setup world/scene
    _world = new QGraphicsScene();
    _world->setBackgroundBrush(QBrush(QColor(0, 0, 0)));

    setScene(_world);

    setInteractive(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // setup game engine
    QObject::connect(&_engine, SIGNAL(timeout()),
                     this, SLOT(nextFrame()));
    QObject::connect(this, SIGNAL(gameOver()),
        this, SLOT(gameOverSlot()), Qt::QueuedConnection);
    QObject::connect(this, SIGNAL(changeEngine()),
        this, SLOT(changeEngineSlot()));
    _engine.setTimerType(Qt::PreciseTimer);
    _engine.setInterval(1000 / GAME_FPS);

    setWindowIcon(QIcon(":/icons/peggle.ico"));
    setWindowTitle("Peggle");

    _builder = new LevelBuilder();
    _window = new WindowBuilder();
    _score = 0;
    _secondScore = 0;
    _redPegHit = -1;
    _simulationScore = 0;
    _character = Character::NONE;
    _power = false;

    reset();
    init();
    
}

void Game::reset()
{
    _score = 0;
    alpha = 89;
    simulationScore.clear();
    _secondScore = 0;
    remainingBall = 10;
    _redPegHit = -1;
    _character = Character::NONE;
    _secondCharacter = Character::NONE;
    restoreGreen = false;
    _power = false;
    turn = true;
    trajectory.resize(30);
    for (auto el : trajectory)
        delete el;
    
    _engine.stop();
    _world->clear();

}

void Game::init()
{
    reset();
    
    background= _world->addPixmap(QPixmap(Sprites::instance()->get("peggle_title")));
    
    centerOn(background);
    setSceneRect(0, 0, getBackground()->sceneBoundingRect().width(), getBackground()->sceneBoundingRect().height());
    showNormal();
    _state = GameState::TITLE;
}


void Game::menuDuel()
{
    _world->clear();
    _state = GameState::TITLE;
    showNormal();
    fitInView(_world->addPixmap(QPixmap(Sprites::instance()->get("peggle_title"))), Qt::KeepAspectRatio);

}

void Game::buildLevel()
{
    _world->clear();
    
    _builder->load("level_1");

}

void Game::play() //in gioco
{
    
    showFullScreen();
    

    buildLevel();
    
    _engine.start();
   
    _state = GameState::PLAYING;
    setMouseTracking(true);
    
}

void Game::nextFrame()
{
    
    world2d->Step(timeStep, velocityIterations, positionIterations); 

  for (b2ContactEdge* edge = MasterPegBox->GetContactList(); edge; edge = edge->next)
    {
      
      if (edge->contact->IsTouching()&& static_cast<Peg*>(edge->contact->GetFixtureA()->GetBody()->GetUserData())) {
          // if is a Peg
          if (edge->contact->GetFixtureA()->GetBody()->GetType() == b2BodyType::b2_staticBody) {
              Peg* tmp = static_cast<Peg*>(edge->contact->GetFixtureA()->GetBody()->GetUserData());
              if (simulation) {
                  if (!tmp->getSimulHit())
                      tmp->hit();
              }
              else {
                  if (!tmp->getHitted())
                      tmp->hit();
              }
              
           // if is Bucket
          }else if (edge->contact->GetFixtureA()->GetBody()->GetType() == b2BodyType::b2_kinematicBody){
              Bucket* tmp = static_cast<Bucket*>(edge->contact->GetFixtureA()->GetBody()->GetUserData());
              tmp->goal();
          }

      }
    }
  if (getPower() && (_character == Character::BEAVER|| _character == Character::RABBIT)) {
      for (b2ContactEdge* edge = secondMasterPegBox->GetContactList(); edge; edge = edge->next)
      {

          if (edge->contact->IsTouching() && static_cast<Peg*>(edge->contact->GetFixtureA()->GetBody()->GetUserData())) {
              // if is a Peg
              if (edge->contact->GetFixtureA()->GetBody()->GetType() == b2BodyType::b2_staticBody) {
                  Peg* tmp = static_cast<Peg*>(edge->contact->GetFixtureA()->GetBody()->GetUserData());
                  if (simulation) {
                      if (!tmp->getSimulHit())
                          tmp->hit();
                  }
                  else {
                      if (!tmp->getHitted())
                          tmp->hit();
                  }
                 
              }
          }
      }
  }

    //master peg
   
  if (!simulation) {
      masterPegGraphic->advance(MasterPegBox);
      bucketGraphic->advance(BucketBox);
      if(_power&&((turn ? getCharacter() : getSecondCharacter()) ==Character::BEAVER|| (turn ? getCharacter() : getSecondCharacter()) == Character::RABBIT))
        secondMasterPegGraphics->advance(secondMasterPegBox);
  }
  else {
      masterPegGraphic->simulAdvance(MasterPegBox);
  }
       
}

// EVENTI
void Game::mousePressEvent(QMouseEvent* e)
{
    
    if (!simulation) {
        if (_state ==GameState::TITLE)
        {
            
            _window->load("mode");
            return;
        }

        if (_state == GameState::MODE || _state == GameState::RESULT_DOUBLE||_state == GameState::RESULT_SINGLE|| _state == GameState::SELECT_SINGLE_CHARACTER || _state == GameState::SELECT_FIRST_CHARACTER || _state == GameState::SELECT_SECOND_CHARACTER || _state == GameState::SELECT_DIFFICULTY)
            QGraphicsView::mousePressEvent(e);
        

        if (e->button() == Qt::LeftButton && _state == GameState::PLAYING)
        {
            if (_mode == GameMode::CPU&& !turn) {
                return;
            }
            Game::instance()->setPower(false);
            
            masterPegGraphic->setFire(true);
            world2d->SetGravity(b2Vec2(0, 25.0f));
            QPoint midPos((sceneRect().width() / 2), 0), currPos;
            for (auto el : trajectory)
                if (dynamic_cast<MasterPeg*>(el))
                    el->setVisible(false);
            currPos = QPoint(mapToScene(e->pos()).x(), mapToScene(e->pos()).y());
            QVector2D p = QVector2D(currPos.x() - midPos.x(), currPos.y() - midPos.y());
            p.normalize();
            MasterPegBox->SetLinearVelocity(b2Vec2(p.x() * 15, p.y() * 15));
            masterPegGraphic->setVisible(true);
            cannon->setPixmap(Sprites::instance()->get("cannon_without_ball"));
            QSound::play(":/sounds/cannonshot.wav");

        }

        if (e->button() == Qt::RightButton)
            _engine.setInterval(5);
    }
}

void Game::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::RightButton&& !simulation)
        _engine.setInterval(1000 / GAME_FPS);
    
}

void Game::mouseMoveEvent(QMouseEvent* e)
{
    if (_state==GameState::PLAYING){
        if (!simulation) {
            QPointF midPos((sceneRect().width() / 2), 0), currPos;
                
                currPos = QPoint(mapToScene(e->pos()).x(), mapToScene(e->pos()).y());
                setMouseTracking(true);
                if (turn) {
                    switch (_character) {
                    case Character::UNICORN:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("unicorn_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("unicorn_face_right")));
                        break;
                    case Character::BEAVER:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("beaver_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("beaver_face_right")));
                        break;
                    case Character::CRAB:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("crab_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("crab_face_right")));
                        break;
                    case Character::FLOWER:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("flower_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("flower_face_right")));
                        break;
                    case Character::PUMPKIN:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("pumpkin_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("pumpkin_face_right")));
                        break;
                    case Character::ALIEN:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("alien_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("alien_face_right")));
                        break;
                    case Character::OWL:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("owl_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("owl_face_right")));
                        break;
                    case Character::DRAGON:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("dragon_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("dragon_face_right")));
                        break;
                    case Character::RABBIT:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("rabbit_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("rabbit_face_right")));
                        break;
                    }
                }
                else {
                    switch (_secondCharacter) {
                    case Character::UNICORN:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("unicorn_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("unicorn_face_right")));
                        break;
                    case Character::BEAVER:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("beaver_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("beaver_face_right")));
                        break;
                    case Character::CRAB:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("crab_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("crab_face_right")));
                        break;
                    case Character::FLOWER:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("flower_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("flower_face_right")));
                        break;
                    case Character::PUMPKIN:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("pumpkin_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("pumpkin_face_right")));
                        break;
                    case Character::ALIEN:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("alien_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("alien_face_right")));
                        break;
                    case Character::OWL:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("owl_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("owl_face_right")));
                        break;
                    case Character::DRAGON:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("dragon_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("dragon_face_right")));
                        break;
                    case Character::RABBIT:
                        if (currPos.x() < midPos.x())
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("rabbit_face_left")));
                        else
                            character_face->setPixmap(QPixmap(Sprites::instance()->get("rabbit_face_right")));
                        break;
                    }
                }
                                   
                QPointF center(720, 100);
                QLineF v1(center, QPoint(720, 500));

                QLineF v2(center, currPos);
                v2.setLength(200.0);
                if (currPos.y()>104) {

                    if (!masterPegGraphic->getFire())
                        MasterPegBox->SetTransform(b2Vec2(v2.p2().x() / 30.0, v2.p2().y() / 30.0), MasterPegBox->GetAngle());

                    cannon->setTransformOriginPoint(QPoint(30, -65));
                    cannon->setRotation(-v1.angleTo(v2));
                }
                if ((turn?getCharacter():getSecondCharacter()) == Character::UNICORN && getPower()) {
                    QVector2D p = QVector2D(currPos.x() - midPos.x(), currPos.y() - midPos.y());
                    p.normalize();
                    for (auto el : trajectory)
                        delete el;
                    trajectory.resize(30);
                    if (!masterPegGraphic->getFire()) {
                        for (int i = 10; i < 30; i++) { // three seconds at 60fps
                            if (i % 2 == 0)
                                continue;
                            b2Vec2 trajectoryPosition = getTrajectoryPoint(b2Vec2(MasterPegBox->GetPosition().x, MasterPegBox->GetPosition().y), b2Vec2(p.x() * 15, p.y() * 15), i);
                            trajectory.push_back(new MasterPeg(QPoint((trajectoryPosition.x * 30.0), (trajectoryPosition.y * 30.0))));
                        }
                    }
                }
        }
    }
}

void Game::wheelEvent(QWheelEvent* e)
{
    if (e->angleDelta().y() > 0)
        scale(1.1, 1.1);
    else
        scale(1 / 1.1, 1 / 1.1);
}

void Game::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_S && _state == GameState::TITLE)
    {
        play();
    }
    if (e->key() == Qt::Key_P && (_state == GameState::PLAYING || _state == GameState::PAUSED))
    {
        printScore();
        togglePause();
    }
    if (e->key() == Qt::Key_R && _state == GameState::PLAYING)
    {
        init();
    }
    if (e->key() == Qt::Key_A && _state == GameState::PLAYING)
    {
        simulationScore.clear();
        alpha = 89;
        fire(alpha);
    }

}

void Game::resizeEvent(QResizeEvent* e)
{
    fitInView(0, 0, sceneRect().width(), sceneRect().height(), Qt::KeepAspectRatio);
   
    QGraphicsView::resizeEvent(e);
}

// UTILITY
void Game::togglePause()
{
    if (_state == GameState::PLAYING)
    {
        paused->setVisible(true);
        _engine.stop();
        _state = GameState::PAUSED;
    }
    else if (_state == GameState::PAUSED)
    {
        paused->setVisible(false);
        _engine.start();
        _state = GameState::PLAYING;
    }
}


void Game::updateFPS()
{
    _FPS_label->setText(QString("FPS = ") + QString::number(_frame_count));
    _frame_count = 0;

    // setup FPS display and measuring
    _FPS_label = new QLabel("FPS =           ", this);
    _FPS_label->setFont(QFont("Consolas", 10));
    _FPS_label->move(QPoint(40, 5));
    QObject::connect(&_FPS_timer, SIGNAL(timeout()), this, SLOT(updateFPS()));
    _FPS_timer.setTimerType(Qt::PreciseTimer);
    _FPS_timer.setInterval(1000);
    _FPS_timer.start();
}

b2Vec2 Game::getTrajectoryPoint(b2Vec2& startingPosition, b2Vec2& startingVelocity, float n)
{
    //velocity and gravity are given per second but we want time step values here
    float t = 1 / 60.0f; // seconds per time step (at 60fps)
    b2Vec2 stepVelocity = t * startingVelocity; // m/s
    
    b2Vec2 stepGravity = t * t * b2Vec2(0,25.0); // m/s/s

    return startingPosition + n * stepVelocity + 0.5f * (n * n + n) * stepGravity;
}


void Game::printRemainingBall(int b) { 
    std::string tmp = "";
    switch (b)
    {
    case 10:
        tmp = "10";
        break;
    case 9:
        tmp = "9";
        break;
    case 8:
        tmp = "8";
        break;
    case 7:
        tmp = "7";
        break;
    case 6:
        tmp = "6";
        break;
    case 5:
        tmp = "5";
        break;
    case 4:
        tmp = "4";
        break;
    case 3:
        tmp = "3";
        break;
    case 2:
        tmp = "2";
        break;
    case 1:
        tmp = "1";
        break;
    case 0:
        tmp = "0";
        break;
    };
    remainingBallPixmap->setPixmap(Sprites::instance()->get(tmp));
    remainingBallPixmap->setPos(QPoint(60 -remainingBallPixmap->boundingRect().width() / 2, 180));
}


void Game::clearHittedPeg() {
    for (int i = 0; i < PegBox.size(); i++) {
       
        Peg* tmp = static_cast<Peg*>(PegBox[i]->GetUserData());
        if (tmp->getHitted()) {
            tmp->setVisible(false);
            PegBox[i]->DestroyFixture(PegBox[i]->GetFixtureList());
            
        }
    }
    if (restoreGreen) {

        int i = 0;

        do {
            i = rand() % 95;
        } while (static_cast<Peg*>(PegBox[i]->GetUserData())->getPegColor() == PegColor::RED && (static_cast<Peg*>(PegBox[i]->GetUserData())->getHitted()) && !(static_cast<Peg*>(PegBox[i]->GetUserData())->isVisible()));
        static_cast<Peg*>(PegBox[i]->GetUserData())->changeColor(PegColor::GREEN);
        restoreGreen = false;
    }
    
}

void Game::addMolt() {
    _redPegHit++; 
    molt[_redPegHit]->setVisible(true); 
    if (_redPegHit == 9)
        molt_x[0]->setVisible(true);
    else if(_redPegHit == 14) 
        molt_x[1]->setVisible(true);
    else if (_redPegHit == 18)//18
        molt_x[2]->setVisible(true);
    else if (_redPegHit == 21)
        molt_x[3]->setVisible(true);
}

void Game::save() {
    QJsonObject recordObject;
    recordObject.insert("RemainingBall", QJsonValue::fromVariant(remainingBall));
    recordObject.insert("RedPegHit", QJsonValue::fromVariant(_redPegHit));

    QJsonObject RemainingPeg;
    for (int i = 0; i < PegBox.size(); i++) {
        QJsonObject tmp;
        tmp.insert("X", PegBox[i]->GetPosition().x);
        tmp.insert("Y", PegBox[i]->GetPosition().y);
        tmp.insert("Color", static_cast<Peg*>(PegBox[i]->GetUserData())->getPegColor()==PegColor::RED?true:false);
        RemainingPeg.insert(QString::number(i), tmp);
    }
    recordObject.insert("RemainingPeg", RemainingPeg);

 
    QJsonDocument doc(recordObject);
    qDebug().noquote() << doc.toJson();

    QFile file("C:/Users/achil/Desktop/Prova.json");
    file.open(QFile::WriteOnly | QFile::Text | QFile::Truncate);
    file.write(doc.toJson());
    file.close();
}

void Game::fire(float alfa) {
        // -90 == 0
       
        _engine.setInterval(0.1);

        simulation = true;
        alfa = alfa - 90;
        QPoint midPos((sceneRect().width() / 2), 130);

        QLineF p = QLineF(midPos, QPointF((sceneRect().width() / 2), 500));
        QLineF f = QLineF(midPos, QPointF((sceneRect().width() / 2), 300));
        QLineF c = p;
        
        p.setAngle(alfa);
        f.setAngle(alfa);
        QVector2D z = QVector2D(p.dx(), p.dy());
        z.normalize();
      


        cannon->setTransformOriginPoint(QPoint(30, -65));
        cannon->setRotation(-c.angleTo(p));
        MasterPegBox->SetTransform(b2Vec2(f.p2().x() / 30.0, f.p2().y() / 30.0), MasterPegBox->GetAngle());

        MasterPegBox->SetLinearVelocity(b2Vec2(z.x()*15, z.y() * 15));
       
        world2d->SetGravity(b2Vec2(0, 25.0f));
        masterPegGraphic->setFire(true);
        
    
    
}


void Game::activePower(Character c) {
    setPower(true);
    switch (c) {
    case Character::FLOWER:
    {
        QSound::play(":/sounds/powerup_flower.wav");

        int twenty = (25 - Game::instance()->getRedPegHit()) * 20 / 100;
        int c = 0;
        for (int i = 0; i < 95; i++) {
            if (static_cast<Peg*>(Game::instance()->PegBox[i]->GetUserData())->getPegColor() == PegColor::RED && c < twenty && !static_cast<Peg*>(Game::instance()->PegBox[i]->GetUserData())->getHitted()) {
                printf("Twenty");
                static_cast<Peg*>(Game::instance()->PegBox[i]->GetUserData())->hit();
                c++;
            }
        }
        Game::instance()->setPower(false);
    }
    break;
    case Character::ALIEN:
    {
        QSound::play(":/sounds/powerup.wav");

        QPoint centerCircle(masterPegGraphic->pos().x(), masterPegGraphic->pos().y());
        QList<QGraphicsItem*> tmp;
        for (auto el : PegBox) {
            if (static_cast<Peg*>(el->GetUserData())->getPegColor() == PegColor::GREEN) {
                for (auto obj : PegBox) {
                    if (!(static_cast<Peg*>(obj->GetUserData())->getHitted())) {
                        int x = static_cast<Peg*>(obj->GetUserData())->pos().x() - static_cast<Peg*>(el->GetUserData())->pos().x();
                        int y = static_cast<Peg*>(obj->GetUserData())->pos().y() - static_cast<Peg*>(el->GetUserData())->pos().y();
                        int sqrt = std::sqrt((x * x) + (y * y));
                        if (sqrt <= 100)
                            static_cast<Peg*>(obj->GetUserData())->hit();
                    }
                }
                
            }
        }
        
        Game::instance()->setPower(false);
    }
    break;
    case Character::BEAVER:
    {
        QSound::play(":/sounds/powerup.wav");

        b2BodyDef ballDef;
        ballDef.type = b2_dynamicBody;
        ballDef.linearDamping = 0;
        ballDef.position.Set(getMasterPegGraphic()->pos().x() / 30.0, getMasterPegGraphic()->pos().y() / 30.0);

        secondMasterPegBox = world2d->CreateBody(&ballDef);

        secondMasterPegGraphics = new MasterPeg(QPoint(ballDef.position.x * 30.0, ballDef.position.y * 30.0));

        secondMasterPegBox->SetUserData((secondMasterPegGraphics));

        // Shape
        b2CircleShape ballShape;
        ballShape.m_p.Set(0, 0);
        ballShape.m_radius = 0.2;

        // Fixture
        b2FixtureDef ballFixtureDef;
        ballFixtureDef.restitution = 0.7;
        ballFixtureDef.shape = &ballShape;
        ballFixtureDef.density = 50.0f;

        secondMasterPegBox->CreateFixture(&ballFixtureDef);
        secondMasterPegBox->SetLinearVelocity(b2Vec2(5, -5));
        secondMasterPegBox->SetAngularVelocity(0);
        world2d->SetGravity(b2Vec2(0, 25.0f));
        break;

    }

    case Character::RABBIT:
    {
        QSound::play(":/sounds/powerup_rabbit.wav");

        int  r = rand() % 3;
        switch (r) {
        case 0:
            activePower(Character::ALIEN);
            break;
        case 1:
            activePower(Character::BEAVER);
            break;
 
        case 2:
            activePower(Character::FLOWER);
            break;
        }
    case Character::UNICORN:
    
        QSound::play(":/sounds/powerup.wav");
    
    break;
    case Character::DRAGON:
    
        QSound::play(":/sounds/powerup.wav");
    
    case Character::PUMPKIN:
    
        QSound::play(":/sounds/powerup.wav");
    
    break;
    case Character::OWL:
    
        QSound::play(":/sounds/powerup.wav");
    
    }
    }
}

void Game::printScore() {
    QVector<int> arrOne;
    QVector<int> arrTwo;
    int x = _score;
    int y = _secondScore;
    if (x == 0)
        for (int i = 0; i < 6; i++) 
            scoreGraphics[i]->setPixmap(QPixmap(Sprites::instance()->get("0-score")).scaled(50, 50));
    

    while (x> 0)
    {
        arrOne.push_back(x % 10);
        x /= 10;
    }
    while (y > 0)
    {
        arrTwo.push_back(y % 10);
        y /= 10;
    }

      for (int i = 0; i < arrOne.length(); i++)
        scoreGraphics[i]->setPixmap(QPixmap(Sprites::instance()->get(std::to_string(arrOne[i]) + "-score")).scaled(50, 50));

     for (int i = 0; i < arrTwo.length(); i++)
        scoreGraphicsTwo[i]->setPixmap(QPixmap(Sprites::instance()->get(std::to_string(arrTwo[i]) + "-score")).scaled(50, 50));
    
}


void Game::gameOverSlot() {
    QSound::play(":/sounds/fanfare.wav");
    _engine.stop();
    if(_mode==GameMode::SINGLE)
        _window->load("result_single");
    else 
        _window->load("result_double");
}

void Game::changeEngineSlot() {
    _engine.setInterval(1000 / GAME_FPS);
}