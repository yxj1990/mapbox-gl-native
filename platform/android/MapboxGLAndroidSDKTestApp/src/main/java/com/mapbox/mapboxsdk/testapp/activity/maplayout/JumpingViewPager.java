package com.mapbox.mapboxsdk.testapp.activity.maplayout;

import android.content.Context;
import android.support.v4.view.ViewPager;
import android.util.AttributeSet;

public class JumpingViewPager extends ViewPager{

  public JumpingViewPager(Context context) {
    super(context);
  }

  public JumpingViewPager(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  @Override
  public void setCurrentItem(int item) {
    super.setCurrentItem(item);
    setCurrentItem(item, false);
  }
}
