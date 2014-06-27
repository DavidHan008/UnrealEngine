// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "TextEditHelper.h"
#include "SlateWordWrapper.h"

void SMultiLineEditableText::FCursorInfo::SetCursorLocationAndCalculateAlignment(const TSharedPtr<FTextLayout>& TextLayout, const FTextLocation& InCursorPosition, const ECursorEOLMode CursorEOLMode)
{
	FTextLocation NewCursorPosition = InCursorPosition;
	ECursorAlignment NewAlignment = ECursorAlignment::Left;

	const int32 CursorLineIndex = InCursorPosition.GetLineIndex();
	const int32 CursorOffset = InCursorPosition.GetOffset();

	// A CursorOffset of zero could mark the end of an empty line, but we don't need to adjust the cursor for an empty line
	if (TextLayout.IsValid() && CursorOffset > 0)
	{
		bool bHasFoundEOL = false;

		if (CursorEOLMode == ECursorEOLMode::HardLines)
		{
			const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();
			const FTextLayout::FLineModel& Line = Lines[CursorLineIndex];
			bHasFoundEOL = (Line.Text->Len() == CursorOffset);
		}
		else
		{
			const TArray< FTextLayout::FLineView >& LineViews = TextLayout->GetLineViews();
			const int32 LineViewIndex = TextLayout->GetLineViewIndexForTextLocation(LineViews, InCursorPosition);

			if (LineViews.IsValidIndex(LineViewIndex))
			{
				const FTextLayout::FLineView& CurrentLineView = LineViews[LineViewIndex];
				bHasFoundEOL = (CurrentLineView.Range.EndIndex == CursorOffset);
			}
		}

		if (bHasFoundEOL)
		{
			// We need to move the cursor back one from where it currently is; this keeps the interaction point the same 
			// (since the cursor is aligned to the right), but visually draws the cursor in the correct place
			NewCursorPosition = FTextLocation(NewCursorPosition, -1);
			NewAlignment = ECursorAlignment::Right;
		}
	}

	SetCursorLocationAndAlignment(NewCursorPosition, NewAlignment);
}

void SMultiLineEditableText::FCursorInfo::SetCursorLocationAndAlignment(const FTextLocation& InCursorPosition, const ECursorAlignment InCursorAlignment)
{
	CursorPosition = InCursorPosition;
	CursorAlignment = InCursorAlignment;
	LastCursorInteractionTime = FSlateApplication::Get().GetCurrentTime();
}

SMultiLineEditableText::FCursorInfo SMultiLineEditableText::FCursorInfo::CreateUndo() const
{
	FCursorInfo UndoData;
	UndoData.CursorPosition = CursorPosition;
	UndoData.CursorAlignment = CursorAlignment;
	return UndoData;
}

void SMultiLineEditableText::FCursorInfo::RestoreFromUndo(const FCursorInfo& UndoData)
{
	// Use SetCursorLocationAndAlignment since it updates LastCursorInteractionTime
	SetCursorLocationAndAlignment(UndoData.CursorPosition, UndoData.CursorAlignment);
}

SMultiLineEditableText::FSlateCursorRunHighlighter::FSlateCursorRunHighlighter(const FCursorInfo* InCursorInfo)
	: CursorInfo(InCursorInfo)
{
	check(CursorInfo);
}

void SMultiLineEditableText::FSlateCursorRunHighlighter::OnArrangeChildren( const TSharedRef< ILayoutBlock >& Block, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const 
{
	//No Widgets
}

FChildren* SMultiLineEditableText::FSlateCursorRunHighlighter::GetChildren()
{
	static FNoChildren NoChildrenInstance;
	return &NoChildrenInstance;
}

int32 SMultiLineEditableText::FSlateCursorRunHighlighter::OnPaint( const FTextLayout::FLineView& Line, const TSharedRef< ISlateRun >& Run, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const 
{
	LayerId = OnPaintCursor(Line, Run, Block, DefaultStyle, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled, CursorInfo->GetCursorAlignment());

	return Run->OnPaint( Line, Block, DefaultStyle, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );
}

int32 SMultiLineEditableText::FSlateCursorRunHighlighter::OnPaintCursor( const FTextLayout::FLineView& Line, const TSharedRef< ISlateRun >& Run, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled, const ECursorAlignment InCursorAlignment ) const
{
	FVector2D Location( Block->GetLocationOffset() );
	Location.Y = Line.Offset.Y;

	FLinearColor CursorColorAndOpacity = InWidgetStyle.GetForegroundColor();

	const float FontMaxCharHeight = FTextEditHelper::GetFontHeight(DefaultStyle.Font);
	const float CursorWidth = FTextEditHelper::CalculateCaretWidth(FontMaxCharHeight);
	const double CurrentTime = FSlateApplication::Get().GetCurrentTime();

	// The cursor is always visible (i.e. not blinking) when we're interacting with it; otherwise it might get lost.
	const bool bForceCursorVisible = (CurrentTime - CursorInfo->GetLastCursorInteractionTime()) < EditableTextDefs::CaretBlinkPauseTime;
	float CursorOpacity = (bForceCursorVisible)
		? 1.0f
		: FMath::RoundToFloat( FMath::MakePulsatingValue( CurrentTime, EditableTextDefs::BlinksPerSecond ));

	CursorOpacity *= CursorOpacity;	// Squared falloff, because it looks more interesting
	CursorColorAndOpacity.A = CursorOpacity;

	// @todo: Slate Styles - make this brush part of the widget style
	const FSlateBrush* StyleInfo = FCoreStyle::Get().GetBrush("EditableText.SelectionBackground");

	const FVector2D OptionalWidth = InCursorAlignment == ECursorAlignment::Right ? FVector2D(Block->GetSize().X, 0) : FVector2D::ZeroVector;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		FPaintGeometry(AllottedGeometry.AbsolutePosition + Location + OptionalWidth, FVector2D(CursorWidth * AllottedGeometry.Scale, Line.TextSize.Y), AllottedGeometry.Scale),
		StyleInfo,
		MyClippingRect,
		bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
		CursorColorAndOpacity*InWidgetStyle.GetColorAndOpacityTint());

	return LayerId;
}

TSharedRef< SMultiLineEditableText::FSlateCursorRunHighlighter > SMultiLineEditableText::FSlateCursorRunHighlighter::Create(const FCursorInfo* InCursorInfo)
{
	return MakeShareable( new SMultiLineEditableText::FSlateCursorRunHighlighter(InCursorInfo) );
}

SMultiLineEditableText::FSlateSelectionRunHighlighter::FSlateSelectionRunHighlighter(const FCursorInfo* InCursorInfo)
	: SMultiLineEditableText::FSlateCursorRunHighlighter(InCursorInfo)
	, CursorAlignment(ECursorAlignment::Left)
{
}

int32 SMultiLineEditableText::FSlateSelectionRunHighlighter::OnPaint( const FTextLayout::FLineView& Line, const TSharedRef< ISlateRun >& Run, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const 
{
	FVector2D Location( Block->GetLocationOffset() );
	Location.Y = Line.Offset.Y;

	const FColor SelectionBackgroundColorAndOpacity = ((FLinearColor::White - DefaultStyle.ColorAndOpacity.GetColor(InWidgetStyle))*0.5f + FLinearColor(-0.2f, -0.05f, 0.15f)) * InWidgetStyle.GetColorAndOpacityTint();

	float HighlightWidth = Block->GetSize().X;
	if (HighlightWidth == 0)
	{
		const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FVector2D SpaceSize = FontMeasureService->Measure(FString(TEXT("  ")), DefaultStyle.Font);
		HighlightWidth = SpaceSize.X;
	}

	// Draw the actual highlight rectangle
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		++LayerId,
		FPaintGeometry(AllottedGeometry.AbsolutePosition + Location, FVector2D(HighlightWidth, Line.Size.Y), AllottedGeometry.Scale),
		&DefaultStyle.HighlightShape,
		MyClippingRect,
		bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
		SelectionBackgroundColorAndOpacity
	);

	//FLinearColor InvertedForeground = FLinearColor::White - InWidgetStyle.GetForegroundColor();
	//InvertedForeground.A = InWidgetStyle.GetForegroundColor().A;

	//FWidgetStyle WidgetStyle( InWidgetStyle );
	//WidgetStyle.SetForegroundColor( InvertedForeground );

	// If the cursor is within the range of this line view, we need to draw it
	const FTextLocation CursorLocation = CursorInfo->GetCursorLocation();
	if(Line.ModelIndex == CursorLocation.GetLineIndex() && Line.Range.InclusiveContains(CursorLocation.GetOffset()))
	{
		LayerId = OnPaintCursor(Line, Run, Block, DefaultStyle, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled, CursorAlignment);
	}

	return Run->OnPaint( Line, Block, DefaultStyle, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );
}

TSharedRef< SMultiLineEditableText::FSlateSelectionRunHighlighter > SMultiLineEditableText::FSlateSelectionRunHighlighter::Create(const FCursorInfo* InCursorInfo)
{
	return MakeShareable( new SMultiLineEditableText::FSlateSelectionRunHighlighter(InCursorInfo) );
}

SMultiLineEditableText::SMultiLineEditableText()
	: PreferredCursorScreenOffsetInLine(0)
	, bSelectAllTextWhenFocused(false)
	, bIsDragSelecting(false)
	, bWasFocusedByLastMouseDown(false)
	, bHasDragSelectedSinceFocused(false)
	, CurrentUndoLevel(INDEX_NONE)
	, bIsChangingText(false)
	, IsReadOnly(false)
	, UICommandList(new FUICommandList())
{
	
}

void SMultiLineEditableText::Construct( const FArguments& InArgs )
{
	CursorInfo = FCursorInfo();
	SelectionStart = TOptional<FTextLocation>();
	PreferredCursorScreenOffsetInLine = 0;

	WrapTextAt = InArgs._WrapTextAt;
	AutoWrapText = InArgs._AutoWrapText;
	CachedAutoWrapTextWidth = 0.0f;

	Margin = InArgs._Margin;
	LineHeightPercentage = InArgs._LineHeightPercentage;
	Justification = InArgs._Justification;
	TextStyle = *InArgs._TextStyle;
	
	if (InArgs._Font.IsSet() || InArgs._Font.IsBound())
	{
		TextStyle.SetFont(InArgs._Font.Get());
	}

	IsReadOnly = InArgs._IsReadOnly;

	OnTextChanged = InArgs._OnTextChanged;
	OnTextCommitted = InArgs._OnTextCommitted;

	CursorRunHighlighter = FSlateCursorRunHighlighter::Create(&CursorInfo);
	SelectionRunHighlighter = FSlateSelectionRunHighlighter::Create(&CursorInfo);
	TextLayout = FSlateTextLayout::Create();

	BoundText = InArgs._Text;

	{
		const FText& TextToSet = BoundText.Get(FText::GetEmpty());
		SetEditableText(TextToSet, true); // force the set to ensure we create an empty run (if BoundText returns an empty string)

		// Update the cached BoundText value to prevent it triggering another SetEditableText update again next Tick
		if (BoundText.IsBound())
		{
			BoundTextLastTick = TextToSet;
		}
	}

	TextLayout->SetWrappingWidth( WrapTextAt.Get() );
	TextLayout->SetMargin( Margin.Get() );
	TextLayout->SetLineHeightPercentage( LineHeightPercentage.Get() );
	TextLayout->SetJustification( Justification.Get() );

	TextLayout->UpdateIfNeeded();

	// Map UI commands to delegates which are called when the command should be executed
	UICommandList->MapAction(FGenericCommands::Get().Undo,
		FExecuteAction::CreateSP(this, &SMultiLineEditableText::Undo),
		FCanExecuteAction::CreateSP(this, &SMultiLineEditableText::CanExecuteUndo));

	UICommandList->MapAction(FGenericCommands::Get().Cut,
		FExecuteAction::CreateSP(this, &SMultiLineEditableText::CutSelectedTextToClipboard),
		FCanExecuteAction::CreateSP(this, &SMultiLineEditableText::CanExecuteCut));

	UICommandList->MapAction(FGenericCommands::Get().Paste,
		FExecuteAction::CreateSP(this, &SMultiLineEditableText::PasteTextFromClipboard),
		FCanExecuteAction::CreateSP(this, &SMultiLineEditableText::CanExecutePaste));

	UICommandList->MapAction(FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &SMultiLineEditableText::CopySelectedTextToClipboard),
		FCanExecuteAction::CreateSP(this, &SMultiLineEditableText::CanExecuteCopy));

	UICommandList->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SMultiLineEditableText::ExecuteDeleteAction),
		FCanExecuteAction::CreateSP(this, &SMultiLineEditableText::CanExecuteDelete));

	UICommandList->MapAction(FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateSP(this, &SMultiLineEditableText::SelectAllText),
		FCanExecuteAction::CreateSP(this, &SMultiLineEditableText::CanExecuteSelectAll));

	// build context menu extender
	MenuExtender = MakeShareable(new FExtender);
	MenuExtender->AddMenuExtension("EditText", EExtensionHook::Before, TSharedPtr<FUICommandList>(), InArgs._ContextMenuExtender);
}

void SMultiLineEditableText::SetText(const TAttribute< FText >& InText)
{
	// NOTE: This will unbind any getter that is currently assigned to the Text attribute!  You should only be calling
	//       SetText() if you know what you're doing.
	BoundText = InText;

	const FText& TextToSet = BoundText.Get(FText::GetEmpty());

	// Update the cached BoundText value to prevent it triggering another SetEditableText update again next Tick
	if (BoundText.IsBound())
	{
		BoundTextLastTick = TextToSet;
	}

	// Update the internal editable text
	if (SetEditableText(TextToSet))
	{
		// Let outsiders know that the text content has been changed
		OnTextChanged.ExecuteIfBound(TextToSet);
	}
}

bool SMultiLineEditableText::SetEditableText(const FText& TextToSet, const bool bForce)
{
	FText EditedText;
	TextLayout->GetAsText(EditedText);

	const bool HasTextChanged = !EditedText.ToString().Equals(TextToSet.ToString(), ESearchCase::CaseSensitive);
	if (HasTextChanged || bForce)
	{
		const FString& TextToSetString = TextToSet.ToString();
		const int32 TextToSetLength = TextToSetString.Len();

		TextLayout->ClearLines();

		if(TextToSetLength)
		{
			// Iterate over each line break candidate, adding ranges from after the end of the last line added to before the newline or end of string.
			FLineBreakIterator LBI(TextToSetString);
			int32 RangeBegin = LBI.GetCurrentPosition();
			TSharedPtr< FString > LineText;

			for (;;)
			{
				const int32 BreakIndex = LBI.MoveToNext();

				// If beginning or potential end is invalid, cease.
				if (RangeBegin == INDEX_NONE || BreakIndex == INDEX_NONE)
				{
					break;
				}

				const int32 BreakingCharacterIndex = BreakIndex - 1;
				if (BreakingCharacterIndex >= RangeBegin) // Valid index check.
				{
					const TCHAR BreakingCharacter = TextToSetString[BreakingCharacterIndex];
					// If line break candidate is after a newline, add the range as a new line.
					if (FChar::IsLinebreak(BreakingCharacter))
					{
						LineText = MakeShareable(new FString(BreakingCharacterIndex - RangeBegin, (*TextToSetString) + RangeBegin));
					}
					// If the line break candidate is the end of the string, add the range as a new line.
					else if (BreakIndex == TextToSetLength)
					{
						LineText = MakeShareable(new FString(BreakIndex - RangeBegin, (*TextToSetString) + RangeBegin));
					}
					else
					{
						continue;
					}

					if (LineText.IsValid())
					{
						// Create run.
						TArray< TSharedRef< IRun > > Runs;
						Runs.Add(FSlateTextRun::Create(LineText.ToSharedRef(), TextStyle));

						TextLayout->AddLine(LineText.ToSharedRef(), Runs);

						// Reset the LineText so we don't add it multiple times.
						LineText.Reset();
					}

					RangeBegin = BreakIndex; // The next line begins after the end of the current line.
				}
			}
		}
		else
		{
			TSharedPtr< FString > LineText = MakeShareable(new FString());

			// Create an empty run
			TArray< TSharedRef< IRun > > Runs;
			Runs.Add(FSlateTextRun::Create(LineText.ToSharedRef(), TextStyle));

			TextLayout->AddLine(LineText.ToSharedRef(), Runs);
		}

		return true;
	}

	return false;
}

FReply SMultiLineEditableText::OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent )
{
	UpdateCursorHighlight();
	return SWidget::OnKeyboardFocusReceived( MyGeometry, InKeyboardFocusEvent );
}

void SMultiLineEditableText::OnKeyboardFocusLost( const FKeyboardFocusEvent& InKeyboardFocusEvent )
{
	// Skip the focus lost code if it's due to the context menu opening
	if (!ContextMenuWindow.IsValid())
	{
		//ITextInputMethodSystem* const TextInputMethodSystem = FSlateApplication::Get().GetTextInputMethodSystem();
		//if (TextInputMethodSystem)
		//{
		//	TextInputMethodSystem->DeactivateContext(TextInputMethodContext.ToSharedRef());
		//}

		// When focus is lost let anyone who is interested that text was committed
		if (InKeyboardFocusEvent.GetCause() != EKeyboardFocusCause::WindowActivate) //Don't clear selection when activating a new window or cant copy and paste on right click )
		{
			ClearSelection();
		}

		// See if user explicitly tabbed away or moved focus
		ETextCommit::Type TextAction;
		switch (InKeyboardFocusEvent.GetCause())
		{
		case EKeyboardFocusCause::Keyboard:
		case EKeyboardFocusCause::Mouse:
			TextAction = ETextCommit::OnUserMovedFocus;
			break;

		case EKeyboardFocusCause::Cleared:
			TextAction = ETextCommit::OnCleared;
			break;

		default:
			TextAction = ETextCommit::Default;
			break;
		}

		//Always clear the local undo chain on commit.
		ClearUndoStates();

		FText EditedText;
		TextLayout->GetAsText(EditedText);

		OnTextCommitted.ExecuteIfBound(EditedText, TextAction);
		RemoveCursorHighlight();
	}
}

void SMultiLineEditableText::StartChangingText()
{
	// Never change text on read only controls! 
	check(!IsReadOnly.Get());

	// We're starting to (potentially) change text
	check(!bIsChangingText);
	bIsChangingText = true;

	// Save off an undo state in case we actually change the text
	MakeUndoState(StateBeforeChangingText);
}

void SMultiLineEditableText::FinishChangingText()
{
	// We're no longer changing text
	check(bIsChangingText);
	bIsChangingText = false;

	FText EditedText;
	TextLayout->GetAsText(EditedText);

	// Has the text changed?
	const bool HasTextChanged = !EditedText.ToString().Equals(StateBeforeChangingText.Text.ToString(), ESearchCase::CaseSensitive);
	if (HasTextChanged)
	{
		// Save text state
		SaveText(EditedText);

		// Text was actually changed, so push the undo state we previously saved off
		PushUndoState(StateBeforeChangingText);

		// We're done with this state data now.  Clear out any old data.
		StateBeforeChangingText = FUndoState();

		TextLayout->UpdateIfNeeded();

		// Let outsiders know that the text content has been changed
		OnTextChanged.ExecuteIfBound(EditedText);
	}
}

void SMultiLineEditableText::SaveText(const FText& TextToSave)
{
	// Don't set text if the text attribute has a 'getter' binding on it, otherwise we'd blow away
	// that binding.  If there is a getter binding, then we'll assume it will provide us with
	// updated text after we've fired our 'text changed' callbacks
	if (!BoundText.IsBound())
	{
		BoundText.Set(TextToSave);
	}
}

bool SMultiLineEditableText::GetIsReadOnly() const
{
	return IsReadOnly.Get();
}

void SMultiLineEditableText::BackspaceChar()
{
	if( AnyTextSelected() )
	{
		// Delete selected text
		DeleteSelectedText();
	}
	else
	{
		const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
		FTextLocation FinalCursorLocation = CursorInteractionPosition;

		const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();
		const FTextLayout::FLineModel& Line = Lines[CursorInteractionPosition.GetLineIndex()];

		//If we are at the very beginning of the line...
		if (CursorInteractionPosition.GetOffset() == 0)
		{
			//And if the current line isn't the very first line then...
			if (CursorInteractionPosition.GetLineIndex() > 0)
			{
				int32 CachePreviousLinesCurrentLength = Lines[CursorInteractionPosition.GetLineIndex() - 1].Text->Len();
				if (TextLayout->JoinLineWithNextLine(CursorInteractionPosition.GetLineIndex() - 1))
				{
					//Update the cursor so it appears at the end of the previous line,
					//as we're going to delete the imaginary \n separating them
					FinalCursorLocation = FTextLocation(CursorInteractionPosition.GetLineIndex() - 1, CachePreviousLinesCurrentLength);
				}
			}
			//else do nothing as the FinalCursorLocation is already correct
		}
		//else if (CursorInteractionPosition.GetOffset() == Line.Text->Len())
		//{
		//	//And if the current line isn't the very last line then...
		//	if (Lines.IsValidIndex(CursorInteractionPosition.GetLineIndex() + 1))
		//	{
		//		//do nothing to the cursor as the FinalCursorLocation is already correct
		//		TextLayout->JoinLineWithNextLine(CursorInteractionPosition.GetLineIndex());
		//	}
		//	//else do nothing as the FinalCursorLocation is already correct
		//}
		else
		{
			// Delete character to the left of the caret
			if (TextLayout->RemoveAt(FTextLocation(CursorInteractionPosition, -1)))
			{
				// Adjust caret position one to the left
				FinalCursorLocation = FTextLocation(CursorInteractionPosition, -1);
			}
		}

		CursorInfo.SetCursorLocationAndCalculateAlignment(TextLayout, FinalCursorLocation);

		ClearSelection();
		UpdateCursorHighlight();
	}
}

/**
 * This gets executed by the context menu and should only attempt to delete any selected text
 */
void SMultiLineEditableText::ExecuteDeleteAction()
{
	if (!IsReadOnly.Get() && AnyTextSelected())
	{
		StartChangingText();

		// Delete selected text
		DeleteSelectedText();

		FinishChangingText();
	}
}

bool SMultiLineEditableText::CanExecuteDelete() const
{
	bool bCanExecute = true;

	// Can't execute if this is a read-only control
	if (IsReadOnly.Get())
	{
		bCanExecute = false;
	}

	// Can't execute unless there is some text selected
	if (!AnyTextSelected())
	{
		bCanExecute = false;
	}

	return bCanExecute;
}

void SMultiLineEditableText::DeleteChar()
{
	if( !IsReadOnly.Get() )
	{
		if( AnyTextSelected() )
		{
			// Delete selected text
			DeleteSelectedText();
		}
		else
		{
			const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
			FTextLocation FinalCursorLocation = CursorInteractionPosition;

			const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();
			const FTextLayout::FLineModel& Line = Lines[CursorInteractionPosition.GetLineIndex()];

			//If we are at the very beginning of the line...
			if (Line.Text->Len() == 0)
			{
				//And if the current line isn't the very first line then...
				if (CursorInteractionPosition.GetLineIndex() > 0)
				{
					if (TextLayout->RemoveLine(CursorInteractionPosition.GetLineIndex()))
					{
						//Update the cursor so it appears at the end of the previous line,
						//as we're going to delete the imaginary \n separating them
						FinalCursorLocation = FTextLocation(CursorInteractionPosition.GetLineIndex() - 1, Lines[CursorInteractionPosition.GetLineIndex() - 1].Text->Len());
					}
				}
				//else do nothing as the FinalCursorLocation is already correct
			}
			else if (CursorInteractionPosition.GetOffset() >= Line.Text->Len())
			{
				//And if the current line isn't the very last line then...
				if (Lines.IsValidIndex(CursorInteractionPosition.GetLineIndex() + 1))
				{
					if (TextLayout->JoinLineWithNextLine(CursorInteractionPosition.GetLineIndex()))
					{
						//else do nothing as the FinalCursorLocation is already correct
					}
				}
				//else do nothing as the FinalCursorLocation is already correct
			}
			else
			{
				// Delete character to the right of the caret
				TextLayout->RemoveAt(CursorInteractionPosition);
				//do nothing to the cursor as the FinalCursorLocation is already correct
			}

			CursorInfo.SetCursorLocationAndCalculateAlignment(TextLayout, FinalCursorLocation);

			ClearSelection();
			UpdateCursorHighlight();
		}
	}
}

bool SMultiLineEditableText::CanTypeCharacter(const TCHAR CharInQuestion) const
{
	return true;
}

void SMultiLineEditableText::TypeChar( const int32 Character )
{
	if( AnyTextSelected() )
	{
		// Delete selected text
		DeleteSelectedText();
	}

	// Certain characters are not allowed
	bool bIsCharAllowed = true;
	{
		if( Character <= 0x1F )
		{
			bIsCharAllowed = false;
		}
	}

	if( bIsCharAllowed )
	{
		const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
		const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();
		const FTextLayout::FLineModel& Line = Lines[ CursorInteractionPosition.GetLineIndex() ];

		// Insert character at caret position
		TextLayout->InsertAt( CursorInteractionPosition, Character );

		// Advance caret position
		ClearSelection();
		const FTextLocation FinalCursorLocation = FTextLocation( CursorInteractionPosition.GetLineIndex(), FMath::Min( CursorInteractionPosition.GetOffset() + 1, Line.Text->Len() ) );

		CursorInfo.SetCursorLocationAndCalculateAlignment(TextLayout, FinalCursorLocation);
		UpdateCursorHighlight();
	}
}

void SMultiLineEditableText::DeleteSelectedText()
{
	if( AnyTextSelected() )
	{
		const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
		FTextLocation SelectionLocation = SelectionStart.Get(CursorInteractionPosition);
		FTextSelection Selection(SelectionLocation, CursorInteractionPosition);

		int32 SelectionBeginningLineIndex = Selection.GetBeginning().GetLineIndex();
		int32 SelectionBeginningLineOffset = Selection.GetBeginning().GetOffset();

		int32 SelectionEndLineIndex = Selection.GetEnd().GetLineIndex();
		int32 SelectionEndLineOffset = Selection.GetEnd().GetOffset();

		if (SelectionBeginningLineIndex == SelectionEndLineIndex)
		{
			TextLayout->RemoveAt(FTextLocation(SelectionBeginningLineIndex, SelectionBeginningLineOffset), SelectionEndLineOffset - SelectionBeginningLineOffset);
			//Do nothing to the cursor as it is already in the correct location
		}
		else
		{
			const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();
			const FTextLayout::FLineModel& EndLine = Lines[SelectionEndLineIndex];

			if (EndLine.Text->Len() == SelectionEndLineOffset)
			{ 
				TextLayout->RemoveLine(SelectionEndLineIndex);
			}
			else
			{
				TextLayout->RemoveAt(FTextLocation(SelectionEndLineIndex, 0), SelectionEndLineOffset);
			}

			for (int32 LineIndex = SelectionEndLineIndex - 1; LineIndex > SelectionBeginningLineIndex; LineIndex--)
			{
				TextLayout->RemoveLine(LineIndex);
			}

			const FTextLayout::FLineModel& BeginLine = Lines[SelectionBeginningLineIndex];

			if (SelectionBeginningLineOffset == 0)
			{
				TextLayout->RemoveLine(SelectionBeginningLineIndex);
			}
			else
			{
				TextLayout->RemoveAt(FTextLocation(SelectionBeginningLineIndex, SelectionBeginningLineOffset), BeginLine.Text->Len() - SelectionBeginningLineOffset);
			}

			TextLayout->JoinLineWithNextLine(SelectionBeginningLineIndex);

			if (Lines.Num() == 0)
			{
				const TSharedRef< FString > EmptyText = MakeShareable(new FString());
				TArray< TSharedRef< IRun > > Runs;
				Runs.Add(FSlateTextRun::Create(EmptyText, TextStyle));

				TextLayout->AddLine(EmptyText, Runs);
			}
		}

		// Clear selection
		ClearSelection();
		const FTextLocation FinalCursorLocation = FTextLocation(SelectionBeginningLineIndex, SelectionBeginningLineOffset);
		CursorInfo.SetCursorLocationAndCalculateAlignment(TextLayout, FinalCursorLocation);
		UpdateCursorHighlight();
	}
}

FReply SMultiLineEditableText::MoveCursor( ECursorMoveMethod::Type Method, const FVector2D& Direction, ECursorAction::Type Action )
{
	bool bAllowMoveCursor = true;
	FTextLocation NewCursorPosition;
	FTextLocation CursorPosition = CursorInfo.GetCursorInteractionLocation();

	// If we have selected text, the cursor needs to:
	// a) Jump to the start of the selection if moving the cursor Left or Up
	// b) Jump to the end of the selection if moving the cursor Right or Down
	// This is done regardless of whether the selection was made left-to-right, or right-to-left
	// This also needs to be done *before* moving to word boundaries, or moving vertically, as the 
	// start point needs to be the start or end of the selection depending on the above rules
	if (Action == ECursorAction::MoveCursor && Method != ECursorMoveMethod::ScreenPosition && AnyTextSelected())
	{
		if (Method == ECursorMoveMethod::CharacterHorizontal)
		{
			// If we're moving the cursor horizontally, we just snap to the start or end of the selection rather than 
			// move the cursor by the normal movement rules
			bAllowMoveCursor = false;
		}

		// Work out which edge of the selection we need to start at
		bool bSnapToSelectionStart = false;
		switch (Method)
		{
		case ECursorMoveMethod::CharacterHorizontal:
		case ECursorMoveMethod::Word:
			bSnapToSelectionStart = Direction.X < 0.0f;
			break;
	
		case ECursorMoveMethod::CharacterVertical:
			bSnapToSelectionStart = Direction.Y < 0.0f;
			break;

		default:
			break;
		}

		// Adjust the current cursor position - also set the new cursor position so that the bAllowMoveCursor == false case is handled
		const FTextSelection Selection(SelectionStart.GetValue(), CursorPosition);
		CursorPosition = bSnapToSelectionStart ? Selection.GetBeginning() : Selection.GetEnd();
		NewCursorPosition = CursorPosition;

		// If we're snapping to a word boundary, but the selection was already at a word boundary, don't let the cursor move any more
		if (Method == ECursorMoveMethod::Word && IsAtWordStart(NewCursorPosition))
		{
			bAllowMoveCursor = false;
		}
	}

	ECursorEOLMode CursorEOLMode = ECursorEOLMode::HardLines;
	if (bAllowMoveCursor)
	{
		switch (Method)
		{
		case ECursorMoveMethod::CharacterHorizontal:
			NewCursorPosition = TranslatedLocation( CursorPosition, Direction.X );
			PreferredCursorScreenOffsetInLine = TextLayout->GetLocationAt( NewCursorPosition ).X;
			break;

		case ECursorMoveMethod::Word:
			NewCursorPosition = ScanForWordBoundary( CursorPosition, Direction.X );
			PreferredCursorScreenOffsetInLine = TextLayout->GetLocationAt( NewCursorPosition ).X;
			break;
	
		case ECursorMoveMethod::CharacterVertical:
			NewCursorPosition = TranslateLocationVertical( CursorPosition, Direction.Y );
			break;
	
		case ECursorMoveMethod::ScreenPosition:
			NewCursorPosition = TextLayout->GetTextLocationAt( Direction * TextLayout->GetScale() );
			CursorEOLMode = ECursorEOLMode::SoftLines;
			PreferredCursorScreenOffsetInLine = Direction.X;
			break;

		default:
			checkSlow(false, "Unknown ECursorMoveMethod value");
			break;
		}
	}

	if (Action == ECursorAction::SelectText)
	{
		// We are selecting text. Just remember where the selection started.
		// The cursor is implicitly the other endpoint.
		if (!SelectionStart.IsSet())
		{
			this->SelectionStart = CursorPosition;
		}
	}
	else
	{
		// No longer selection text; clear the selection!
		this->ClearSelection();
	}

	CursorInfo.SetCursorLocationAndCalculateAlignment(TextLayout, NewCursorPosition, CursorEOLMode);
	UpdateCursorHighlight();

	return FReply::Handled();
}

void SMultiLineEditableText::UpdateCursorHighlight()
{
	TextLayout->ClearHighlights();

	const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
	const FTextLocation SelectionLocation = SelectionStart.Get( CursorInteractionPosition );

	bool bDrawCursor = true;

	const bool bHasSelection = SelectionLocation != CursorInteractionPosition;
	if ( bHasSelection )
	{
		bDrawCursor = false;

		const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
		const FTextLocation SelectionLocation = SelectionStart.Get( CursorInteractionPosition );

		const FTextSelection Selection( SelectionLocation, CursorInteractionPosition );

		const int32 SelectionBeginningLineIndex = Selection.GetBeginning().GetLineIndex();
		const int32 SelectionBeginningLineOffset = Selection.GetBeginning().GetOffset();
		
		const int32 SelectionEndLineIndex = Selection.GetEnd().GetLineIndex();
		const int32 SelectionEndLineOffset = Selection.GetEnd().GetOffset();

		const bool bCursorIsBeforeSelection = CursorInteractionPosition < SelectionLocation;
		SelectionRunHighlighter->SetCursorAlignment((bCursorIsBeforeSelection) ? ECursorAlignment::Left : ECursorAlignment::Right);

		if ( SelectionBeginningLineIndex == SelectionEndLineIndex )
		{
			const FTextRange Range(SelectionBeginningLineOffset, SelectionEndLineOffset);
			if (!Range.IsEmpty())
			{
				TextLayout->AddHighlight(FTextHighlight(SelectionBeginningLineIndex, Range, SelectionRunHighlighter.ToSharedRef()));
			}
			else
			{
				// We have no selection range for this line, but we're supposed to draw the cursor on this line - let the normal cursor handling take care of that
				bDrawCursor = true;
			}
		}
		else
		{
			const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();

			bool bHasAddedBeginHighlight = false;
			bool bHasAddedEndHighlight = false;

			for (int LineIndex = SelectionBeginningLineIndex; LineIndex <= SelectionEndLineIndex; LineIndex++)
			{
				if ( LineIndex == SelectionBeginningLineIndex )
				{
					check(!bHasAddedBeginHighlight);
					bHasAddedBeginHighlight = true;

					const FTextRange Range(SelectionBeginningLineOffset, Lines[LineIndex].Text->Len());
					if (!Range.IsEmpty())
					{
						TextLayout->AddHighlight(FTextHighlight(LineIndex, Range, SelectionRunHighlighter.ToSharedRef()));
					}
					else if (bCursorIsBeforeSelection)
					{
						// We have no selection range for this line, but we're supposed to draw the cursor on this line - let the normal cursor handling take care of that
						bDrawCursor = true;
					}
				}
				else if ( LineIndex == SelectionEndLineIndex )
				{
					check(!bHasAddedEndHighlight);
					bHasAddedEndHighlight = true;

					const FTextRange Range(0, SelectionEndLineOffset);
					if (!Range.IsEmpty())
					{
						TextLayout->AddHighlight(FTextHighlight(LineIndex, Range, SelectionRunHighlighter.ToSharedRef()));
					}
					else if (!bCursorIsBeforeSelection)
					{
						// We have no selection range for this line, but we're supposed to draw the cursor on this line - let the normal cursor handling take care of that
						bDrawCursor = true;
					}
				}
				else
				{
					const FTextRange Range(0, Lines[LineIndex].Text->Len());
					if (!Range.IsEmpty())
					{
						TextLayout->AddHighlight(FTextHighlight(LineIndex, Range, SelectionRunHighlighter.ToSharedRef()));
					}
				}
			}
		}
	}
	
	if ( bDrawCursor )
	{
		// The cursor mode uses the literal position rather than the interaction position
		const FTextLocation CursorPosition = CursorInfo.GetCursorLocation();

		const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();
		const int32 LineTextLength = Lines[CursorPosition.GetLineIndex()].Text->Len();

		if (LineTextLength == 0)
		{
			TextLayout->AddHighlight(FTextHighlight(CursorPosition.GetLineIndex(), FTextRange(0, 0), CursorRunHighlighter.ToSharedRef()));
		}
		else if (CursorPosition.GetOffset() == LineTextLength)
		{
			TextLayout->AddHighlight(FTextHighlight(CursorPosition.GetLineIndex(), FTextRange(LineTextLength - 1, LineTextLength), CursorRunHighlighter.ToSharedRef()));
		}
		else
		{
			TextLayout->AddHighlight(FTextHighlight(CursorPosition.GetLineIndex(), FTextRange(CursorPosition.GetOffset(), CursorPosition.GetOffset() + 1), CursorRunHighlighter.ToSharedRef()));
		}
	}
}

void SMultiLineEditableText::RemoveCursorHighlight()
{
	TextLayout->ClearHighlights();
}

void SMultiLineEditableText::JumpTo(ETextLocation::Type JumpLocation, ECursorAction::Type Action)
{
	switch (JumpLocation)
	{
		case ETextLocation::BeginningOfLine:
		{
			const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
			const TArray< FTextLayout::FLineView >& LineViews = TextLayout->GetLineViews();
			const int32 CurrentLineViewIndex = TextLayout->GetLineViewIndexForTextLocation(LineViews, CursorInteractionPosition );

			if (LineViews.IsValidIndex(CurrentLineViewIndex))
			{
				const FTextLayout::FLineView& CurrentLineView = LineViews[CurrentLineViewIndex];
				
				const FTextLocation OldCursorPosition = CursorInteractionPosition;
				const FTextLocation NewCursorPosition = FTextLocation(OldCursorPosition.GetLineIndex(), CurrentLineView.Range.BeginIndex);
				
				if (Action == ECursorAction::SelectText)
				{
					this->SelectionStart = OldCursorPosition;
				}
				else
				{
					ClearSelection();
				}

				PreferredCursorScreenOffsetInLine = TextLayout->GetLocationAt(NewCursorPosition).X;

				CursorInfo.SetCursorLocationAndCalculateAlignment(TextLayout, NewCursorPosition);
				UpdateCursorHighlight();
			}
		}
		break;

		case ETextLocation::BeginningOfDocument:
		{
			const FTextLocation OldCursorPosition = CursorInfo.GetCursorInteractionLocation();
			const FTextLocation NewCursorPosition = FTextLocation(0,0);

			if (Action == ECursorAction::SelectText)
			{
				this->SelectionStart = OldCursorPosition;
			}
			else
			{
				ClearSelection();
			}

			PreferredCursorScreenOffsetInLine = TextLayout->GetLocationAt(NewCursorPosition).X;

			CursorInfo.SetCursorLocationAndCalculateAlignment(TextLayout, NewCursorPosition);
			UpdateCursorHighlight();
		}
		break;

		case ETextLocation::EndOfLine:
		{
			const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
			const TArray< FTextLayout::FLineView >& LineViews = TextLayout->GetLineViews();
			const int32 CurrentLineViewIndex = TextLayout->GetLineViewIndexForTextLocation(LineViews, CursorInteractionPosition);

			if (LineViews.IsValidIndex(CurrentLineViewIndex))
			{
				const FTextLayout::FLineView& CurrentLineView = LineViews[CurrentLineViewIndex];

				const FTextLocation OldCursorPosition = CursorInteractionPosition;
				const FTextLocation NewCursorPosition = FTextLocation(OldCursorPosition.GetLineIndex(), CurrentLineView.Range.EndIndex);

				if (Action == ECursorAction::SelectText)
				{
					this->SelectionStart = OldCursorPosition;
				}
				else
				{
					ClearSelection();
				}

				PreferredCursorScreenOffsetInLine = TextLayout->GetLocationAt(NewCursorPosition).X;

				CursorInfo.SetCursorLocationAndCalculateAlignment(TextLayout, NewCursorPosition);
				UpdateCursorHighlight();
			}
		}
		break;
		case ETextLocation::EndOfDocument:
		{
			if (!TextLayout->IsEmpty())
			{
				const FTextLocation OldCursorPosition = CursorInfo.GetCursorInteractionLocation();
				const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();

				const int32 LastLineIndex = Lines.Num() - 1;
				const FTextLocation NewCursorPosition = FTextLocation(LastLineIndex, Lines[LastLineIndex].Text->Len());

				if (Action == ECursorAction::SelectText)
				{
					this->SelectionStart = OldCursorPosition;
				}
				else
				{
					ClearSelection();
				}

				PreferredCursorScreenOffsetInLine = TextLayout->GetLocationAt(NewCursorPosition).X;

				CursorInfo.SetCursorLocationAndCalculateAlignment(TextLayout, NewCursorPosition);
				UpdateCursorHighlight();
			}
		}
		break;
	};
}

void SMultiLineEditableText::ClearSelection()
{
	SelectionStart = TOptional<FTextLocation>();
}

bool SMultiLineEditableText::CanExecuteSelectAll() const
{
	bool bCanExecute = true;

	// Can't select all if string is empty
	if (TextLayout->IsEmpty())
	{
		bCanExecute = false;
	}

	const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();
	const int32 NumberOfLines = Lines.Num();

	// Can't select all if entire string is already selected
	const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
	if (SelectionStart.IsSet() &&
		SelectionStart.GetValue() == FTextLocation(0, 0) &&
		CursorInteractionPosition == FTextLocation(NumberOfLines - 1, Lines[NumberOfLines - 1].Text->Len()))
	{
		bCanExecute = false;
	}

	return bCanExecute;
}

void SMultiLineEditableText::SelectAllText()
{
	if (TextLayout->IsEmpty())
	{
		return;
	}

	const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();
	const int32 NumberOfLines = Lines.Num();

	SelectionStart = FTextLocation(0,0);
	const FTextLocation NewCursorPosition = FTextLocation( NumberOfLines - 1, Lines[ NumberOfLines - 1 ].Text->Len() );
	CursorInfo.SetCursorLocationAndCalculateAlignment(TextLayout, NewCursorPosition);
	UpdateCursorHighlight();
}

bool SMultiLineEditableText::SelectAllTextWhenFocused()
{
	return bSelectAllTextWhenFocused.Get();
}

void SMultiLineEditableText::SelectWordAt(const FVector2D& LocalPosition)
{
	FTextLocation InitialLocation = TextLayout->GetTextLocationAt(LocalPosition * TextLayout->GetScale());
	FTextSelection WordSelection = TextLayout->GetWordAt(InitialLocation);

	FTextLocation WordStart = WordSelection.GetBeginning();
	FTextLocation WordEnd = WordSelection.GetEnd();

	if (WordStart.IsValid() && WordEnd.IsValid())
	{
		// Deselect any text that was selected
		ClearSelection();

		if (WordStart != WordEnd)
		{
			SelectionStart = WordStart;
		}

		const FTextLocation NewCursorPosition = WordEnd;
		CursorInfo.SetCursorLocationAndCalculateAlignment(TextLayout, NewCursorPosition);
		UpdateCursorHighlight();
	}
}

void SMultiLineEditableText::BeginDragSelection() 
{
	bIsDragSelecting = true;
}

bool SMultiLineEditableText::IsDragSelecting() const
{
	return bIsDragSelecting;
}

void SMultiLineEditableText::EndDragSelection() 
{
	bIsDragSelecting = false;
}

bool SMultiLineEditableText::AnyTextSelected() const
{
	const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
	const FTextLocation SelectionPosition = SelectionStart.Get( CursorInteractionPosition );
	return SelectionPosition != CursorInteractionPosition;
}

bool SMultiLineEditableText::IsTextSelectedAt(const FVector2D& LocalPosition) const
{
	const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
	const FTextLocation SelectionPosition = SelectionStart.Get(CursorInteractionPosition);

	if (SelectionPosition == CursorInteractionPosition)
	{
		return false;
	}

	const FTextLocation ClickedPosition = TextLayout->GetTextLocationAt(LocalPosition * TextLayout->GetScale());

	FTextLocation SelectionLocation = SelectionStart.Get(CursorInteractionPosition);
	FTextSelection Selection(SelectionLocation, CursorInteractionPosition);

	int32 SelectionBeginningLineIndex = Selection.GetBeginning().GetLineIndex();
	int32 SelectionBeginningLineOffset = Selection.GetBeginning().GetOffset();

	int32 SelectionEndLineIndex = Selection.GetEnd().GetLineIndex();
	int32 SelectionEndLineOffset = Selection.GetEnd().GetOffset();

	if (SelectionBeginningLineIndex == ClickedPosition.GetLineIndex())
	{
		return SelectionBeginningLineOffset >= ClickedPosition.GetOffset();
	}
	else if (SelectionEndLineIndex == ClickedPosition.GetLineIndex())
	{
		return SelectionEndLineOffset < ClickedPosition.GetOffset();
	}
	
	return SelectionBeginningLineIndex < ClickedPosition.GetLineIndex() && SelectionEndLineIndex > ClickedPosition.GetLineIndex();
}

void SMultiLineEditableText::SetWasFocusedByLastMouseDown( bool Value )
{
	bWasFocusedByLastMouseDown = Value;
}

bool SMultiLineEditableText::WasFocusedByLastMouseDown() const
{
	return bWasFocusedByLastMouseDown;
}

bool SMultiLineEditableText::HasDragSelectedSinceFocused() const
{
	return bHasDragSelectedSinceFocused;
}

void SMultiLineEditableText::SetHasDragSelectedSinceFocused( bool Value )
{
	bHasDragSelectedSinceFocused = Value;
}

FReply SMultiLineEditableText::OnEscape()
{
	return FReply::Unhandled();
}

void SMultiLineEditableText::OnEnter()
{
	if (AnyTextSelected())
	{
		// Delete selected text
		DeleteSelectedText();
	}

	const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
	if (TextLayout->SplitLineAt(CursorInteractionPosition))
	{
		// Adjust the cursor position to be at the beginning of the new line
		const FTextLocation NewCursorPosition = FTextLocation(CursorInteractionPosition.GetLineIndex() + 1, 0);
		CursorInfo.SetCursorLocationAndCalculateAlignment(TextLayout, NewCursorPosition);
	}

	ClearSelection();
	UpdateCursorHighlight();
}

bool SMultiLineEditableText::CanExecuteCut() const
{
	bool bCanExecute = true;

	// Can't execute if this is a read-only control
	if (IsReadOnly.Get())
	{
		bCanExecute = false;
	}

	// Can't execute if there is no text selected
	if (!AnyTextSelected())
	{
		bCanExecute = false;
	}

	return bCanExecute;
}

void SMultiLineEditableText::CutSelectedTextToClipboard()
{
	if (AnyTextSelected())
	{
		StartChangingText();

		const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
		FTextLocation SelectionLocation = SelectionStart.Get(CursorInteractionPosition);
		FTextSelection Selection(SelectionLocation, CursorInteractionPosition);

		// Grab the selected substring
		FString SelectedText;
		TextLayout->GetSelectionAsText(SelectedText, Selection);

		// Copy text to clipboard
		FPlatformMisc::ClipboardCopy(*SelectedText);

		DeleteSelectedText();

		UpdateCursorHighlight();

		FinishChangingText();
	}
}

bool SMultiLineEditableText::CanExecuteCopy() const
{
	bool bCanExecute = true;

	// Can't execute if there is no text selected
	if (!AnyTextSelected())
	{
		bCanExecute = false;
	}

	return bCanExecute;
}

void SMultiLineEditableText::CopySelectedTextToClipboard()
{
	if (AnyTextSelected())
	{
		const FTextLocation CursorInteractionPosition = CursorInfo.GetCursorInteractionLocation();
		FTextLocation SelectionLocation = SelectionStart.Get(CursorInteractionPosition);
		FTextSelection Selection(SelectionLocation, CursorInteractionPosition);

		// Grab the selected substring
		FString SelectedText;
		TextLayout->GetSelectionAsText(SelectedText, Selection);

		// Copy text to clipboard
		FPlatformMisc::ClipboardCopy(*SelectedText);
	}
}

bool SMultiLineEditableText::CanExecutePaste() const
{
	bool bCanExecute = true;

	// Can't execute if this is a read-only control
	if (IsReadOnly.Get())
	{
		bCanExecute = false;
	}

	// Can't paste unless the clipboard has a string in it
	if (!DoesClipboardHaveAnyText())
	{
		bCanExecute = false;
	}

	return bCanExecute;
}

bool SMultiLineEditableText::DoesClipboardHaveAnyText() const
{
	FString ClipboardContent;
	FPlatformMisc::ClipboardPaste(ClipboardContent);
	return ClipboardContent.Len() > 0;
}

void SMultiLineEditableText::PasteTextFromClipboard()
{
	if (!IsReadOnly.Get())
	{
		StartChangingText(); 

		// Delete currently selected text, if any
		DeleteSelectedText();

		// Paste from the clipboard
		FString PastedText;
		FPlatformMisc::ClipboardPaste(PastedText);

		for (int32 CurCharIndex = 0; CurCharIndex < PastedText.Len(); ++CurCharIndex)
		{
			const TCHAR& Character = PastedText.GetCharArray()[CurCharIndex];
			if (Character == '\n')
			{
				OnEnter();
			}
			else
			{
				TypeChar(Character);
			}
		}

		FinishChangingText();
	}
}

bool SMultiLineEditableText::CanExecuteUndo() const
{
	return true;
}

void SMultiLineEditableText::Undo()
{
	if (!IsReadOnly.Get() && UndoStates.Num() > 0/* && !TextInputMethodContext->IsComposing*/)
	{
		// Restore from undo state
		int32 UndoStateIndex;
		if (CurrentUndoLevel == INDEX_NONE)
		{
			// We haven't undone anything since the last time a new undo state was added
			UndoStateIndex = UndoStates.Num() - 1;

			// Store an undo state for the current state (before undo was pressed)
			FUndoState NewUndoState;
			MakeUndoState(NewUndoState);
			PushUndoState(NewUndoState);
		}
		else
		{
			// Move down to the next undo level
			UndoStateIndex = CurrentUndoLevel - 1;
		}

		// Is there anything else to undo?
		if (UndoStateIndex >= 0)
		{
			{
				// NOTE: It's important the no code called here creates or destroys undo states!
				const FUndoState& UndoState = UndoStates[UndoStateIndex];

				SaveText(UndoState.Text);

				if (SetEditableText(UndoState.Text))
				{
					// Let outsiders know that the text content has been changed
					OnTextChanged.ExecuteIfBound(UndoState.Text);
				}

				CursorInfo = UndoState.CursorInfo.CreateUndo();
				SelectionStart = UndoState.SelectionStart;

				UpdateCursorHighlight();
			}

			CurrentUndoLevel = UndoStateIndex;
		}
	}
}

void SMultiLineEditableText::Redo()
{
	// Is there anything to redo?  If we've haven't tried to undo since the last time new
	// undo state was added, then CurrentUndoLevel will be INDEX_NONE
	if (!IsReadOnly.Get() && CurrentUndoLevel != INDEX_NONE /*&& !(TextInputMethodContext->IsComposing)*/)
	{
		const int32 NextUndoLevel = CurrentUndoLevel + 1;
		if (UndoStates.Num() > NextUndoLevel)
		{
			// Restore from undo state
			{
				// NOTE: It's important the no code called here creates or destroys undo states!
				const FUndoState& UndoState = UndoStates[NextUndoLevel];

				SaveText(UndoState.Text);

				if (SetEditableText(UndoState.Text))
				{
					// Let outsiders know that the text content has been changed
					OnTextChanged.ExecuteIfBound(UndoState.Text);
				}

				CursorInfo.RestoreFromUndo(UndoState.CursorInfo);
				SelectionStart = UndoState.SelectionStart;

				UpdateCursorHighlight();
			}

			CurrentUndoLevel = NextUndoLevel;

			if (UndoStates.Num() <= CurrentUndoLevel + 1)
			{
				// We've redone all available undo states
				CurrentUndoLevel = INDEX_NONE;

				// Pop last undo state that we created on initial undo
				UndoStates.RemoveAt(UndoStates.Num() - 1);
			}
		}
	}
}

void SMultiLineEditableText::PushUndoState(const SMultiLineEditableText::FUndoState& InUndoState)
{
	// If we've already undone some state, then we'll remove any undo state beyond the level that
	// we've already undone up to.
	if (CurrentUndoLevel != INDEX_NONE)
	{
		UndoStates.RemoveAt(CurrentUndoLevel, UndoStates.Num() - CurrentUndoLevel);

		// Reset undo level as we haven't undone anything since this latest undo
		CurrentUndoLevel = INDEX_NONE;
	}

	// Cache new undo state
	UndoStates.Add(InUndoState);

	// If we've reached the maximum number of undo levels, then trim our array
	if (UndoStates.Num() > EditableTextDefs::MaxUndoLevels)
	{
		UndoStates.RemoveAt(0);
	}
}

void SMultiLineEditableText::ClearUndoStates()
{
	CurrentUndoLevel = INDEX_NONE;
	UndoStates.Empty();
}

void SMultiLineEditableText::MakeUndoState(SMultiLineEditableText::FUndoState& OutUndoState)
{
	//@todo save and restoring the whole document is not ideal [3/31/2014 justin.sargent]
	FText EditedText;
	TextLayout->GetAsText(EditedText);

	OutUndoState.Text = EditedText;
	OutUndoState.CursorInfo = CursorInfo;
	OutUndoState.SelectionStart = SelectionStart;
}

TSharedRef< SWidget > SMultiLineEditableText::GetWidget()
{
	return SharedThis( this );
}

void SMultiLineEditableText::SummonContextMenu(const FVector2D& InLocation)
{
	// Set the menu to automatically close when the user commits to a choice
	const bool bShouldCloseWindowAfterMenuSelection = true;

#define LOCTEXT_NAMESPACE "EditableTextContextMenu"

	// This is a context menu which could be summoned from within another menu if this text block is in a menu
	// it should not close the menu it is inside
	bool bCloseSelfOnly = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, UICommandList, MenuExtender, bCloseSelfOnly, &FCoreStyle::Get());
	{
		MenuBuilder.BeginSection("EditText", LOCTEXT("Heading", "Modify Text"));
		{
			// Undo
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Undo);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("EditableTextModify2");
		{
			// Cut
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Cut);

			// Copy
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);

			// Paste
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Paste);

			// Delete
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("EditableTextModify3");
		{
			// Select All
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().SelectAll);
		}
		MenuBuilder.EndSection();
	}

#undef LOCTEXT_NAMESPACE

	const bool bFocusImmediatley = false;
	TSharedPtr< SWindow > ContextMenuWindowPinned = FSlateApplication::Get().PushMenu(SharedThis(this), MenuBuilder.MakeWidget(), InLocation, FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu), bFocusImmediatley);
	ContextMenuWindow = ContextMenuWindowPinned;

	// Make sure the window is valid.  It's possible for the parent to already be in the destroy queue, for example if the editable text was configured to dismiss it's window during OnTextCommitted.
	if (ContextMenuWindowPinned.IsValid())
	{
		ContextMenuWindowPinned->SetOnWindowClosed(FOnWindowClosed::CreateSP(this, &SMultiLineEditableText::OnWindowClosed));
	}
}

void SMultiLineEditableText::OnWindowClosed(const TSharedRef<SWindow>&)
{
	// Give this focus when the context menu has been dismissed
	FSlateApplication::Get().SetKeyboardFocus(AsShared(), EKeyboardFocusCause::OtherWidgetLostFocus);
}

void SMultiLineEditableText::LoadText()
{
	SetText(BoundText);
	TextLayout->UpdateIfNeeded();
}

void SMultiLineEditableText::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	TextLayout->SetScale( AllottedGeometry.Scale );

	const bool bShouldAppearFocused = HasKeyboardFocus() || ContextMenuWindow.IsValid();

	if (!bShouldAppearFocused && BoundText.IsBound())
	{
		const FText& TextToSet = BoundText.Get(FText::GetEmpty());
		if (!BoundTextLastTick.ToString().Equals(TextToSet.ToString(), ESearchCase::CaseSensitive))
		{
			// The source text has changed, so update the internal editable text
			SetEditableText(TextToSet);
			BoundTextLastTick = TextToSet;
		}
	}
}

int32 SMultiLineEditableText::OnPaint( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// Update the auto-wrap size now that we have computed paint geometry; won't take affect until text frame
	// Note: This is done here rather than in Tick(), because Tick() doesn't get called while resizing windows, but OnPaint() does
	if ( AutoWrapText.Get() )
	{
		CachedAutoWrapTextWidth = AllottedGeometry.Size.X;
	}

	const ETextJustify::Type TextJustification = TextLayout->GetJustification();

	if ( TextJustification == ETextJustify::Right )
	{
		const FVector2D TextLayoutSize = TextLayout->GetSize();
		const FVector2D Offset( ( AllottedGeometry.Size.X - TextLayoutSize.X ), 0 );
		LayerId = TextLayout->OnPaint( TextStyle, AllottedGeometry.MakeChild( Offset, TextLayoutSize ), MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
	}
	else if ( TextJustification == ETextJustify::Center )
	{
		FVector2D TextLayoutSize = TextLayout->GetSize();
		float LayoutWidth = TextLayoutSize.X;
		if (TextLayoutSize.X == 0)
		{
			const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			TextLayoutSize.X = FontMeasureService->Measure(FString(TEXT("  ")), TextStyle.Font).X;
		}

		const FVector2D Offset((AllottedGeometry.Size.X - TextLayoutSize.X) / 2, 0);
		LayerId = TextLayout->OnPaint( TextStyle, AllottedGeometry.MakeChild( Offset, TextLayoutSize ), MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
	}
	else
	{
		LayerId = TextLayout->OnPaint( TextStyle, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
	}

	return LayerId;
}

void SMultiLineEditableText::CacheDesiredSize()
{
	// Get the wrapping width and font to see if they have changed
	float WrappingWidth = WrapTextAt.Get();

	const FMargin& OurMargin = Margin.Get();

	// Text wrapping can either be used defined (WrapTextAt), automatic (AutoWrapText), or a mixture of both
	// Take whichever has the smallest value (>1)
	if(AutoWrapText.Get() && CachedAutoWrapTextWidth >= 1.0f)
	{
		WrappingWidth = (WrappingWidth >= 1.0f) ? FMath::Min(WrappingWidth, CachedAutoWrapTextWidth) : CachedAutoWrapTextWidth;
	}

	TextLayout->SetWrappingWidth( WrappingWidth );
	TextLayout->SetMargin( OurMargin );
	TextLayout->SetLineHeightPercentage( LineHeightPercentage.Get() );
	TextLayout->SetJustification( Justification.Get() );
	TextLayout->UpdateIfNeeded();

	SWidget::CacheDesiredSize();
}

FVector2D SMultiLineEditableText::ComputeDesiredSize() const
{
	const FSlateFontInfo& FontInfo = TextStyle.Font;
	
	const float FontMaxCharHeight = FTextEditHelper::GetFontHeight(FontInfo);
	const float CaretWidth = FTextEditHelper::CalculateCaretWidth(FontMaxCharHeight);

	// The layouts current margin size. We should not report a size smaller then the margins.
	const FMargin Margin = TextLayout->GetMargin();
	const FVector2D TextLayoutSize = TextLayout->GetSize();
	const float WrappingWidth = WrapTextAt.Get();

	// If a wrapping width has been provided that should be reported as the desired width.
	float DesiredWidth = WrappingWidth > 0 ? WrappingWidth : TextLayoutSize.X;
	DesiredWidth = FMath::Max(Margin.GetTotalSpaceAlong<Orient_Horizontal>(), DesiredWidth);
	DesiredWidth += CaretWidth;

	float DesiredHeight = FMath::Max(Margin.GetTotalSpaceAlong<Orient_Vertical>(), TextLayoutSize.Y);
	DesiredHeight = FMath::Max(DesiredHeight, FontMaxCharHeight);
	
	return FVector2D(DesiredWidth, DesiredHeight);
}

FChildren* SMultiLineEditableText::GetChildren()
{
	return TextLayout->GetChildren();
}

void SMultiLineEditableText::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const ETextJustify::Type TextJustification = TextLayout->GetJustification();

	if ( TextJustification == ETextJustify::Right )
	{
		const FVector2D TextLayoutSize = TextLayout->GetSize();
		const FVector2D Offset( ( AllottedGeometry.Size.X - TextLayoutSize.X ), 0 );
		TextLayout->ArrangeChildren( AllottedGeometry.MakeChild( Offset, TextLayoutSize ), ArrangedChildren );
	}
	else if ( TextJustification == ETextJustify::Center )
	{
		const FVector2D TextLayoutSize = TextLayout->GetSize();
		const FVector2D Offset( ( AllottedGeometry.Size.X - TextLayoutSize.X ) / 2, 0 );
		TextLayout->ArrangeChildren( AllottedGeometry.MakeChild( Offset, TextLayoutSize ), ArrangedChildren );
	}
	else
	{
		TextLayout->ArrangeChildren( AllottedGeometry, ArrangedChildren );
	}
}

bool SMultiLineEditableText::SupportsKeyboardFocus() const
{
	return true;
}

FReply SMultiLineEditableText::OnKeyChar( const FGeometry& MyGeometry,const FCharacterEvent& InCharacterEvent )
{
	return FTextEditHelper::OnKeyChar( InCharacterEvent, SharedThis( this ) );
}

FReply SMultiLineEditableText::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	FReply Reply = FTextEditHelper::OnKeyDown( InKeyboardEvent, SharedThis( this ) );

	// Process keybindings if the event wasn't already handled
	if (!Reply.IsEventHandled() && UICommandList->ProcessCommandBindings(InKeyboardEvent))
	{
		Reply = FReply::Handled();
	}

	return Reply;
}

FReply SMultiLineEditableText::OnKeyUp( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	return FReply::Unhandled();
}

FReply SMultiLineEditableText::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) 
{
	FReply Reply = FTextEditHelper::OnMouseButtonDown( MyGeometry, MouseEvent, SharedThis( this ) );
	return Reply;
}

FReply SMultiLineEditableText::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) 
{
	FReply Reply = FTextEditHelper::OnMouseButtonUp( MyGeometry, MouseEvent, SharedThis( this ) );
	return Reply;
}

FReply SMultiLineEditableText::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FTextEditHelper::OnMouseMove( MyGeometry, MouseEvent, SharedThis( this ) );
	return Reply;
}

FReply SMultiLineEditableText::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply Reply = FTextEditHelper::OnMouseButtonDoubleClick(MyGeometry, MouseEvent, SharedThis(this));
	return Reply;
}

FCursorReply SMultiLineEditableText::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	return FCursorReply::Cursor( EMouseCursor::TextEditBeam );
}

/** Remember where the cursor was when we started selecting. */
void SMultiLineEditableText::BeginSelecting( const FTextLocation& Endpoint )
{
	SelectionStart = Endpoint;
}

FTextLocation SMultiLineEditableText::TranslatedLocation( const FTextLocation& Location, int8 Direction ) const
{
	const int32 OffsetInLine = Location.GetOffset() + Direction;
	const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();
	const int32 NumberOfLines = Lines.Num();
	const int32 LineTextLength = Lines[ Location.GetLineIndex() ].Text->Len();
		
	if ( Location.GetLineIndex() < NumberOfLines - 1 && OffsetInLine > LineTextLength )
	{
		// We're going over the end of the line and we aren't on the last line
		return FTextLocation( Location.GetLineIndex() + 1, 0 );
	}
	else if ( OffsetInLine < 0 && Location.GetLineIndex() > 0 )
	{
		// We're stepping before the beginning of the line, and we're not on the first line.
		const int32 NewLineIndex = Location.GetLineIndex() - 1;
		return FTextLocation( NewLineIndex, Lines[ NewLineIndex ].Text->Len() );	
	}
	else
	{
		if ( LineTextLength == 0 )
		{
			return FTextLocation( Location.GetLineIndex(), 0 );
		}

		return FTextLocation( Location.GetLineIndex(), FMath::Clamp( OffsetInLine, 0, LineTextLength ) );
	}
}

FTextLocation SMultiLineEditableText::TranslateLocationVertical( const FTextLocation& Location, int8 Direction ) const
{
	const TArray< FTextLayout::FLineView >& LineViews = TextLayout->GetLineViews();
	const int32 NumberOfLineViews = LineViews.Num();

	const int32 CurrentLineViewIndex = TextLayout->GetLineViewIndexForTextLocation(LineViews, Location);
	ensure(CurrentLineViewIndex != INDEX_NONE);
	const FTextLayout::FLineView& CurrentLineView = LineViews[ CurrentLineViewIndex ];

	const int32 NewLineViewIndex = FMath::Clamp( CurrentLineViewIndex + Direction, 0, NumberOfLineViews - 1 );
	const FTextLayout::FLineView& NewLineView = LineViews[ NewLineViewIndex ];
	
	// Our horizontal position is the clamped version of whatever the user explicitly set with horizontal movement.
	return TextLayout->GetTextLocationAt( NewLineView, FVector2D( PreferredCursorScreenOffsetInLine, NewLineView.Offset.Y ) * TextLayout->GetScale() );
}

FTextLocation SMultiLineEditableText::ScanForWordBoundary( const FTextLocation& CurrentLocation, int8 Direction ) const
{
	FTextLocation Location = TranslatedLocation(CurrentLocation, Direction);

	while ( !IsAtBeginningOfDocument(Location) && !IsAtBeginningOfLine(Location) && !IsAtEndOfDocument(Location) && !IsAtEndOfLine(Location) && !IsAtWordStart(Location) )
	{
		Location = TranslatedLocation(Location, Direction);
	}	

	return Location;
}

TCHAR SMultiLineEditableText::GetCharacterAt( const FTextLocation& Location ) const
{
	const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();

	const bool bIsLineEmpty = Lines[ Location.GetLineIndex() ].Text->IsEmpty();
	return (bIsLineEmpty)
		? '\n'
		: (*Lines[ Location.GetLineIndex() ].Text )[Location.GetOffset()];
}

bool SMultiLineEditableText::IsAtBeginningOfDocument( const FTextLocation& Location ) const
{
	return Location.GetLineIndex() == 0 && Location.GetOffset() == 0;
}
	
bool SMultiLineEditableText::IsAtEndOfDocument( const FTextLocation& Location ) const
{
	const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();
	const int32 NumberOfLines = Lines.Num();

	return NumberOfLines == 0 || ( NumberOfLines - 1 == Location.GetLineIndex() && Lines[ Location.GetLineIndex() ].Text->Len()-1 == Location.GetOffset() );
}

bool SMultiLineEditableText::IsAtBeginningOfLine( const FTextLocation& Location ) const
{
	return Location.GetOffset() == 0;
}

bool SMultiLineEditableText::IsAtEndOfLine( const FTextLocation& Location ) const
{
	const TArray< FTextLayout::FLineModel >& Lines = TextLayout->GetLineModels();
	return Lines[ Location.GetLineIndex() ].Text->Len()-1 == Location.GetOffset();
}

bool SMultiLineEditableText::IsAtWordStart( const FTextLocation& Location ) const
{
	const FTextSelection WordUnderCursor = TextLayout->GetWordAt(Location);
	const FTextLocation WordStart = WordUnderCursor.GetBeginning();

	return WordStart.IsValid() && WordStart == Location;
}
