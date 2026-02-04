/****************************************************************************
 Copyright (c) 2012 cocos2d-x.org
 Copyright (c) 2010 Sangwoo Im
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

 https://axmol.dev/

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "TableView.h"
#include "TableViewCell.h"

NS_AX_EXT_BEGIN

void TableViewDelegate::tableCellHighlight(TableView* /*table*/, TableViewCell* /*cell*/)
{}

void TableViewDelegate::tableCellUnhighlight(TableView* /*table*/, TableViewCell* /*cell*/)
{}

void TableViewDelegate::tableCellWillRecycle(TableView* /*table*/, TableViewCell* /*cell*/)
{}

Size TableViewDataSource::tableCellSizeForIndex(TableView* table, ssize_t /*idx*/)
{
    return cellSizeForTable(table);
}

Size TableViewDataSource::cellSizeForTable(TableView* /*table*/)
{
    return Size::ZERO;
}

TableView* TableView::create()
{
    return TableView::create(nullptr, Size::ZERO);
}

TableView* TableView::create(TableViewDataSource* dataSource, Size size)
{
    return TableView::create(dataSource, size, nullptr);
}

TableView* TableView::create(TableViewDataSource* dataSource, Size size, Node* container)
{
    TableView *table = new (std::nothrow) TableView();
    table->initWithViewSize(size, container);
    table->autorelease();
    table->setDataSource(dataSource);
    table->_updateCellPositions();
    table->_updateContentSize();

    return table;
}

bool TableView::initWithViewSize(Size size, Node* container/* = nullptr*/)
{
    if (ScrollView::initWithViewSize(size,container))
    {
        _indices.clear();
        _verticalFillOrder = VerticalFillOrder::BOTTOM_UP;
        _horizontalFillOrder = HorizontalFillOrder::LEFT_TO_RIGHT;
        this->setDirection(Direction::VERTICAL);

        ScrollView::setDelegate(this);
        return true;
    }
    return false;
}

TableView::TableView()
: _touchedCell(nullptr)
, _dataSource(nullptr)
, _tableViewDelegate(nullptr)
, _oldDirection(Direction::NONE)
, _isUsedCellsDirty(false)
{

}

TableView::~TableView()
{
    for (const auto& cell : _cellsFreed)
    {
        cell->cleanup();
    }
}

void TableView::setVerticalFillOrder(VerticalFillOrder fillOrder)
{
    if (_verticalFillOrder != fillOrder)
    {
        _verticalFillOrder = fillOrder;
        if (!_cellsUsed.empty())
        {
            this->reloadData();
        }
    }
}

void TableView::setHorizontalFillOrder(HorizontalFillOrder order)
{
    if (_horizontalFillOrder != order)
    {
        _horizontalFillOrder = order;
        if (!_cellsUsed.empty())
        {
            this->reloadData();
        }
    }
}

void TableView::updateCellAtIndex(ssize_t idx)
{
    if (idx == AX_INVALID_INDEX)
    {
        return;
    }
    long countOfItems = _dataSource->numberOfCellsInTableView(this);
    if (0 == countOfItems || idx > countOfItems-1)
    {
        return;
    }

    TableViewCell* cell = this->cellAtIndex(idx);
    if (cell)
    {
        this->_moveCellOutOfSight(cell);
    }
    cell = _dataSource->tableCellAtIndex(this, idx);
    this->_setIndexForCell(idx, cell);
    this->_addCellIfNecessary(cell);
    _isDirty = true;
}

void TableView::insertCellAtIndex(ssize_t idx)
{
    if (idx == AX_INVALID_INDEX)
    {
        return;
    }

    long countOfItems = _dataSource->numberOfCellsInTableView(this);
    if (0 == countOfItems || idx > countOfItems-1)
    {
        return;
    }

    long newIdx = 0;

    auto cell = cellAtIndex(idx);
    if (cell)
    {
        newIdx = _cellsUsed.getIndex(cell);
        // Move all cells behind the inserted position
        for (long i = newIdx; i < _cellsUsed.size(); i++)
        {
            cell = _cellsUsed.at(i);
            this->_setIndexForCell(cell->getIdx()+1, cell);
        }
    }

    //insert a new cell
    cell = _dataSource->tableCellAtIndex(this, idx);
    this->_setIndexForCell(idx, cell);
    this->_addCellIfNecessary(cell);

    this->_updateCellPositions();
    this->_updateContentSize();
}

void TableView::removeCellAtIndex(ssize_t idx)
{
    if (idx == AX_INVALID_INDEX)
    {
        return;
    }

    long uCountOfItems = _dataSource->numberOfCellsInTableView(this);
    if (0 == uCountOfItems || idx > uCountOfItems-1)
    {
        return;
    }

    ssize_t newIdx = 0;

    TableViewCell* cell = this->cellAtIndex(idx);
    if (!cell)
    {
        return;
    }

    newIdx = _cellsUsed.getIndex(cell);

    //remove first
    this->_moveCellOutOfSight(cell);

    _indices.erase(idx);
    this->_updateCellPositions();

    for (ssize_t i = _cellsUsed.size()-1; i > newIdx; i--)
    {
        cell = _cellsUsed.at(i);
        this->_setIndexForCell(cell->getIdx()-1, cell);
    }
}

void TableView::reloadData()
{
    _oldDirection = Direction::NONE;

    // 기존 셀 순서가 보존되도록 뒤에서부터 _cellsFreed에 쌓아준다.
    for (auto it = _cellsUsed.rbegin(); it != _cellsUsed.rend(); it++)
    {
        auto cell = *it;
        if(_tableViewDelegate != nullptr)
        {
            _tableViewDelegate->tableCellWillRecycle(this, cell);
        }
        _cellsFreed.push_back(cell);
        cell->reset();
        if (cell->getParent() == this->getContainer())
        {
            this->getContainer()->removeChild(cell, false);
        }
    }

    _indices.clear();
    _cellsUsed.clear();
    
    this->_updateCellPositions();
    this->_updateContentSize();
    _isDirty = true;
}

TableViewCell* TableView::dequeueCell()
{
    TableViewCell *cell;
    if (_cellsFreed.empty())
    {
        cell = nullptr;
    }
    else
    {
        cell = _cellsFreed.back();
        cell->retain();
        _cellsFreed.pop_back();
        cell->autorelease();
    }
    return cell;
}

TableViewCell* TableView::cellAtIndex(ssize_t idx)
{
    if (_isDirty)
    {
        renderCells();
    }
    
    if (_indices.find(idx) != _indices.end())
    {
        for (const auto& cell : _cellsUsed)
        {
            if (cell->getIdx() == idx)
            {
                return cell;
            }
        }
    }

    return nullptr;
}

void TableView::renderCells()
{
    _isDirty = false;
    long countOfItems = _dataSource->numberOfCellsInTableView(this);
    if (0 == countOfItems)
    {
        return;
    }
    
    if (_isUsedCellsDirty)
    {
        _isUsedCellsDirty = false;
        std::sort(_cellsUsed.begin(), _cellsUsed.end(), [] (TableViewCell* a, TableViewCell* b) {
            return a->getIdx() < b->getIdx();
        });
    }
    
    ssize_t startIdx = 0, endIdx = 0;
    Vec2 offset = this->getContentOffset() * -1;
    const auto maxIdx = std::max<ssize_t>(countOfItems-1, 0);
    
    if (_verticalFillOrder == VerticalFillOrder::TOP_DOWN)
    {
        offset.y = offset.y + _viewSize.height/this->getContainer()->getScaleY();
    }
    if (_horizontalFillOrder == HorizontalFillOrder::RIGHT_TO_LEFT)
    {
        offset.x = offset.x + _viewSize.width/this->getContainer()->getScaleX();
    }
    startIdx = this->_indexFromOffset(offset);
    if (startIdx == AX_INVALID_INDEX)
    {
        startIdx = countOfItems - 1;
    }
    
    if (_verticalFillOrder == VerticalFillOrder::TOP_DOWN)
    {
        offset.y -= _viewSize.height/this->getContainer()->getScaleY();
    }
    else
    {
        offset.y += _viewSize.height/this->getContainer()->getScaleY();
    }
    
    if (_horizontalFillOrder == HorizontalFillOrder::RIGHT_TO_LEFT)
    {
        offset.x -= _viewSize.width/this->getContainer()->getScaleX();
    }
    else
    {
        offset.x += _viewSize.width/this->getContainer()->getScaleX();
    }
    
    endIdx = this->_indexFromOffset(offset);
    if (endIdx == AX_INVALID_INDEX)
    {
        endIdx = countOfItems - 1;
    }
    
    std::list<TableViewCell*> cellsToMoveOutOfSight;
    for (auto cell : _cellsUsed)
    {
        const auto idx = cell->getIdx();
        if (idx < startIdx || (endIdx < idx && idx <= maxIdx))
        {
            cellsToMoveOutOfSight.push_back(cell);
        }
    }
    for (auto cell : cellsToMoveOutOfSight)
    {
        this->_moveCellOutOfSight(cell);
    }
    for (long i = startIdx; i <= endIdx; i++)
    {
        if (_indices.find(i) != _indices.end())
        {
            continue;
        }
        this->updateCellAtIndex(i);
    }
}

void TableView::visit(Renderer* renderer, const Mat4& parentTransform, uint32_t parentFlags)
{
    if (_isDirty)
    {
        renderCells();
    }
    
    ScrollView::visit(renderer, parentTransform, parentFlags);
}

void TableView::scrollViewDidScroll(ScrollView* view)
{
    _isDirty = true;
    if (_dataSource->numberOfCellsInTableView(this) <= 0)
    {
        return;
    }
    if(_tableViewDelegate != nullptr)
    {
        _tableViewDelegate->scrollViewDidScroll(this);
    }
}

bool TableView::onTouchBegan(Touch* pTouch, Event* pEvent)
{
    for (Node* c = this; c != nullptr; c = c->getParent())
    {
        if (!c->isVisible())
        {
            return false;
        }
    }

    bool touchResult = ScrollView::onTouchBegan(pTouch, pEvent);

    if(_touches.size() == 1)
    {
        long index;
        Vec2 point;

        point = this->getContainer()->convertTouchToNodeSpace(pTouch);

        index = this->_indexFromOffset(point);
        if (index == AX_INVALID_INDEX)
        {
            _touchedCell = nullptr;
        }
        else
        {
            _touchedCell  = this->cellAtIndex(index);
        }

        if (_touchedCell && _tableViewDelegate != nullptr)
        {
            _tableViewDelegate->tableCellHighlight(this, _touchedCell);
        }
    }
    else if (_touchedCell)
    {
        if(_tableViewDelegate != nullptr)
        {
            _tableViewDelegate->tableCellUnhighlight(this, _touchedCell);
        }

        _touchedCell = nullptr;
    }

    return touchResult;
}

void TableView::onTouchMoved(Touch* pTouch, Event* pEvent)
{
    ScrollView::onTouchMoved(pTouch, pEvent);

    if (_touchedCell && isTouchMoved())
    {
        if(_tableViewDelegate != nullptr)
        {
            _tableViewDelegate->tableCellUnhighlight(this, _touchedCell);
        }

        _touchedCell = nullptr;
    }
}

void TableView::onTouchEnded(Touch* pTouch, Event* pEvent)
{
    if (!this->isVisible())
    {
        return;
    }

    if (_touchedCell)
    {
        Rect bb = this->getBoundingBox();
        bb.origin = _parent->convertToWorldSpace(bb.origin);

        if (bb.containsPoint(pTouch->getLocation()) && _tableViewDelegate != nullptr)
        {
            _tableViewDelegate->tableCellUnhighlight(this, _touchedCell);
            _tableViewDelegate->tableCellTouched(this, _touchedCell);
        }

        _touchedCell = nullptr;
    }

    ScrollView::onTouchEnded(pTouch, pEvent);
}

void TableView::onTouchCancelled(Touch* pTouch, Event* pEvent)
{
    ScrollView::onTouchCancelled(pTouch, pEvent);

    if (_touchedCell)
    {
        if(_tableViewDelegate != nullptr)
        {
            _tableViewDelegate->tableCellUnhighlight(this, _touchedCell);
        }

        _touchedCell = nullptr;
    }
}

long TableView::__indexFromOffset(Vec2 offset)
{
    long low = 0;
    long high = _vCellsPositions.size() - 2;
    float search;
    switch (this->getDirection())
    {
        case Direction::HORIZONTAL:
            search = offset.x;
            break;
        default:
            search = offset.y;
            break;
    }

    while (high >= low)
    {
        long index = low + (high - low) / 2;
        float cellStart = _vCellsPositions[index];
        float cellEnd = _vCellsPositions[index + 1];

        if (search >= cellStart && search <= cellEnd)
        {
            return index;
        }
        else if (search < cellStart)
        {
            high = index - 1;
        }
        else
        {
            low = index + 1;
        }
    }

    if (low <= 0)
    {
        return 0;
    }

    return -1;
}

long TableView::_indexFromOffset(Vec2 offset)
{
    long index = 0;
    const long maxIdx = _dataSource->numberOfCellsInTableView(this) - 1;

    if (_verticalFillOrder == VerticalFillOrder::TOP_DOWN)
    {
        offset.y = this->getContainer()->getContentSize().height - offset.y;
    }
    if (_horizontalFillOrder == HorizontalFillOrder::RIGHT_TO_LEFT)
    {
        offset.x = this->getContainer()->getContentSize().width - offset.x;
    }
    index = this->__indexFromOffset(offset);
    if (index != -1)
    {
        index = std::max<long>(0, index);
        if (index > maxIdx)
        {
            index = AX_INVALID_INDEX;
        }
    }

    return index;
}

Vec2 TableView::__offsetFromIndex(ssize_t index)
{
    Vec2 offset;

    switch (this->getDirection())
    {
        case Direction::HORIZONTAL:
            offset.set(_vCellsPositions[index], 0.0f);
            break;
        default:
            offset.set(0.0f, _vCellsPositions[index]);
            break;
    }

    return offset;
}

Vec2 TableView::_offsetFromIndex(ssize_t index)
{
    Vec2 offset = this->__offsetFromIndex(index);

    const Size cellSize = _dataSource->tableCellSizeForIndex(this, index);
    if (_verticalFillOrder == VerticalFillOrder::TOP_DOWN)
    {
        offset.y = this->getContainer()->getContentSize().height - offset.y - cellSize.height;
    }
    if (_horizontalFillOrder == HorizontalFillOrder::RIGHT_TO_LEFT)
    {
        offset.x = this->getContainer()->getContentSize().width - offset.x - cellSize.width;
    }
    return offset;
}

void TableView::_moveCellOutOfSight(TableViewCell* cell)
{
    if(_tableViewDelegate != nullptr)
    {
        _tableViewDelegate->tableCellWillRecycle(this, cell);
    }

    _cellsFreed.push_back(cell);
    _cellsUsed.eraseObject(cell);
    _isUsedCellsDirty = true;
    
    _indices.erase(cell->getIdx());
    cell->reset();
    
    if (cell->getParent() == this->getContainer())
    {
        this->getContainer()->removeChild(cell, false);
    }
}

void TableView::_setIndexForCell(ssize_t index, TableViewCell* cell)
{
    cell->setAnchorPoint(Vec2(0.0f, 0.0f));
    cell->setPosition(this->_offsetFromIndex(index));
    cell->setIdx(index);
}

void TableView::_addCellIfNecessary(TableViewCell* cell)
{
    if (cell->getParent() != this->getContainer())
    {
        this->getContainer()->addChild(cell);
    }
    _cellsUsed.pushBack(cell);
    _indices.insert(cell->getIdx());
    _isUsedCellsDirty = true;
}

void TableView::_updateCellPositions()
{
    long cellsCount = _dataSource->numberOfCellsInTableView(this);
    _vCellsPositions.resize(cellsCount + 1, 0.0);

    if (cellsCount > 0)
    {
        float currentPos = 0;
        switch (this->getDirection())
        {
            case Direction::HORIZONTAL:
                if (_horizontalFillOrder == ScrollView::HorizontalFillOrder::RIGHT_TO_LEFT)
                {
                    currentPos = _rightMargin;
                }
                else
                {
                    currentPos = _leftMargin;
                }
                break;
            default:
                if (_verticalFillOrder == VerticalFillOrder::TOP_DOWN)
                {
                    currentPos = _topMargin;
                }
                else
                {
                    currentPos = _bottomMargin;
                }
                break;
        }
        
        Size cellSize;
        for (int i = 0; i < cellsCount; i++)
        {
            _vCellsPositions[i] = currentPos;
            cellSize = _dataSource->tableCellSizeForIndex(this, i);
            switch (this->getDirection())
            {
                case Direction::HORIZONTAL:
                    currentPos += cellSize.width;
                    break;
                default:
                    currentPos += cellSize.height;
                    break;
            }
        }
        _vCellsPositions[cellsCount] = currentPos;//1 extra value allows us to get right/bottom of the last cell
    }
}

void TableView::_updateContentSize()
{
    Size size = Size::ZERO;
    ssize_t cellsCount = _vCellsPositions.size() - 1;

    if (cellsCount > 0)
    {
        float maxPosition = _vCellsPositions[cellsCount];

        switch (this->getDirection())
        {
            case Direction::HORIZONTAL:
                size = Size(maxPosition, _viewSize.height);
                break;
            default:
                size = Size(_viewSize.width, maxPosition);
                break;
        }
    }

    auto vericalMargin = _topMargin;
    auto horizontalMargin = _rightMargin;
    if (_verticalFillOrder == VerticalFillOrder::TOP_DOWN)
    {
        vericalMargin = _bottomMargin;
    }
    if (_horizontalFillOrder == HorizontalFillOrder::RIGHT_TO_LEFT)
    {
        horizontalMargin = _leftMargin;
    }
    this->setContentSize(size + Size(horizontalMargin, vericalMargin));

    if (_oldDirection != _direction)
    {
        if (_direction == Direction::HORIZONTAL)
        {
            this->setContentOffset(Vec2(0,0));
        }
        else
        {
            this->setContentOffset(Vec2(0,this->minContainerOffset().y));
        }
        _oldDirection = _direction;
    }
}

NS_AX_EXT_END
